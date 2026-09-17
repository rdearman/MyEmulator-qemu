;;; mydebug.el --- MyEmulator native debugger integration -*- lexical-binding: t; -*-

;; This package talks only to tools/mydebug --machine.  The interactive
;; mydebug console remains available in a separate comint buffer.

(require 'asm-mode)
(require 'comint)
(require 'json)
(require 'seq)

(defgroup mydebug nil "MyEmulator native debugger." :group 'tools)
(defcustom mydebug-program nil "Path to tools/mydebug." :type '(choice (const nil) file))
(defcustom mydebug-qmp-socket "/tmp/myemulator-debug.sock" "QMP socket used by mydebug." :type 'file)
(defcustom mydebug-symbols-file nil "Default MyEmulator debug-map file." :type '(choice (const nil) file))
(defcustom mydebug-source-face 'highlight "Face for the current guest source line." :type 'face)

(defconst mydebug--directory
  (file-name-directory (or load-file-name buffer-file-name)))

(defvar mydebug--process nil)
(defvar mydebug--buffer "*MyEmulator Debugger*")
(defvar mydebug--responses nil)
(defvar mydebug--partial "")
(defvar mydebug--metadata nil)
(defvar mydebug--metadata-file nil)
(defvar mydebug--pc-overlay nil)
(defvar mydebug--breakpoint-overlays nil)
(defvar mydebug--previous-registers nil)
(defvar mydebug--breakpoints nil)

(defun mydebug--program ()
  (or mydebug-program
      (expand-file-name "../mydebug" mydebug--directory)))

(defun mydebug--filter (process output)
  (when (process-live-p process)
    (setq mydebug--partial (concat mydebug--partial output))
    (let ((lines (split-string mydebug--partial "\n")))
      (setq mydebug--partial (if (string-suffix-p "\n" mydebug--partial) "" (car (last lines))))
      (dolist (line (if (string-suffix-p "\n" output) lines (butlast lines)))
        (unless (string-empty-p line)
          (condition-case err
              (push (json-parse-string line :object-type 'alist :array-type 'list) mydebug--responses)
            (error (message "mydebug: malformed response: %s" err))))))))

(defun mydebug--ensure-process ()
  (unless (process-live-p mydebug--process)
    (setq mydebug--responses nil mydebug--partial "")
    (let ((args (list "--machine" "--qmp" mydebug-qmp-socket)))
      (when mydebug--metadata-file (setq args (append args (list "--symbols" mydebug--metadata-file))))
      (setq mydebug--process
            (make-process :name "mydebug-machine" :buffer mydebug--buffer
                          :command (cons (mydebug--program) args)
                          :connection-type 'pipe :noquery t
                          :filter #'mydebug--filter
                          :sentinel (lambda (p event)
                                      (unless (process-live-p p)
                                        (message "mydebug: backend exited (%s)" (string-trim event))))))))
  mydebug--process)

(defun mydebug--request (command &optional args)
  (let ((process (mydebug--ensure-process)))
    (process-send-string process
                         (concat (json-encode `((command . ,command)
                                                (args . ,(or args (make-hash-table))))) "\n"))
    (while (and (null mydebug--responses) (process-live-p process))
      (accept-process-output process 0.1))
    (or (pop mydebug--responses) (error "mydebug did not return a response"))))

(defun mydebug--get (object key)
  (cdr (assq key object)))

(defun mydebug--bool (value)
  (and value (not (eq value :false))))

(defun mydebug--load-map (file)
  (setq mydebug--metadata-file file)
  (setq mydebug--metadata
        (and file
             (with-temp-buffer
               (insert-file-contents file)
               (json-parse-string (buffer-string) :object-type 'alist :array-type 'list)))))

(defun mydebug--locations () (or (mydebug--get mydebug--metadata 'locations) nil))
(defun mydebug--symbols () (or (mydebug--get mydebug--metadata 'symbols) nil))

(defun mydebug--location-for-address (address)
  (seq-find (lambda (item) (= (mydebug--get item 'address) address)) (mydebug--locations)))

(defun mydebug--source-path (source)
  (let ((path (expand-file-name source (file-name-directory mydebug--metadata-file))))
    (if (file-exists-p path) path source)))

(defun mydebug--same-source-p (source file)
  (let ((candidates (list source
                          (expand-file-name source (file-name-directory mydebug--metadata-file))
                          (expand-file-name source default-directory))))
    (seq-some (lambda (candidate)
                (or (string= candidate file)
                    (and (file-exists-p candidate) (file-exists-p file)
                         (string= (file-truename candidate) (file-truename file)))))
              candidates)))

(defun mydebug--source-address-for-line (file line)
  (mydebug--get
   (seq-find (lambda (item)
               (and (= (mydebug--get item 'line) line)
                    (mydebug--same-source-p (mydebug--get item 'source) file)))
             (mydebug--locations)) 'address))

(defun mydebug--show-location (registers)
  (let* ((pc (mydebug--get registers 'pc))
         (location (mydebug--location-for-address pc)))
    (when (overlayp mydebug--pc-overlay) (delete-overlay mydebug--pc-overlay))
    (when location
      (let ((file (mydebug--source-path (mydebug--get location 'source)))
            (line (mydebug--get location 'line)))
        (when (file-exists-p file)
          (let ((buffer (find-file-noselect file)))
            (with-current-buffer buffer
              (goto-char (point-min)) (forward-line (1- line))
              (setq mydebug--pc-overlay (make-overlay (line-beginning-position)
                                                      (line-beginning-position 2) buffer))
              (overlay-put mydebug--pc-overlay 'face mydebug-source-face)
              (pop-to-buffer buffer))))))))

(defun mydebug--refresh-registers (state)
  (let ((buffer (get-buffer-create "*MyEmulator Registers*"))
        (registers (mydebug--get state 'registers))
        (old mydebug--previous-registers))
    (with-current-buffer buffer
      (let ((inhibit-read-only t))
        (erase-buffer)
        (insert "MyEmulator registers\n\n")
        (dolist (name '(r0 r1 r2 r3 a0 a1 a2 a3 lr sp pc s0))
          (let* ((key (symbol-name name)) (value (mydebug--get registers name))
                 (changed (and old (/= value (mydebug--get old name))))
                 (formatted (if (or (string-prefix-p "r" key) (string= key "s0"))
                                (format "0x%02x" value) (format "0x%04x" value))))
            (insert (format "%-3s %s%s\n" (upcase key) formatted
                            (if changed "  *changed*" "")))))
        (let ((flags (mydebug--get state 'flags)))
          (insert (format "\nZF=%d NF=%d CF=%d OF=%d IPL=%d\n"
                          (if (mydebug--bool (mydebug--get flags 'zf)) 1 0)
                          (if (mydebug--bool (mydebug--get flags 'nf)) 1 0)
                          (if (mydebug--bool (mydebug--get flags 'cf)) 1 0)
                          (if (mydebug--bool (mydebug--get flags 'of)) 1 0)
                          (mydebug--get flags 'ipl))))
        (special-mode)))
    (display-buffer buffer)
    (setq mydebug--previous-registers registers)))

(defun mydebug--refresh-breakpoints ()
  (let ((response (mydebug--request "breakpoints"))
        (buffer (get-buffer-create "*MyEmulator Breakpoints*")))
    (setq mydebug--breakpoints (mydebug--get response 'breakpoints))
    (dolist (overlay mydebug--breakpoint-overlays)
      (when (overlayp overlay) (delete-overlay overlay)))
    (setq mydebug--breakpoint-overlays nil)
    (dolist (item mydebug--breakpoints)
      (let ((location (mydebug--get item 'location)))
        (when location
          (let ((file (mydebug--source-path (mydebug--get location 'source)))
                (line (mydebug--get location 'line)))
            (when (file-exists-p file)
              (let ((source-buffer (find-file-noselect file)))
                (with-current-buffer source-buffer
                  (goto-char (point-min)) (forward-line (1- line))
                  (let ((overlay (make-overlay (line-beginning-position)
                                               (line-beginning-position) source-buffer)))
                    (overlay-put overlay 'before-string (propertize "● " 'face 'font-lock-warning-face))
                    (push overlay mydebug--breakpoint-overlays)))))))))
    (with-current-buffer buffer
      (let ((inhibit-read-only t))
        (erase-buffer) (insert "Num  Address  Symbol / source\n\n")
        (dolist (item mydebug--breakpoints)
          (let ((location (mydebug--get item 'location)))
            (insert (format "%-4d 0x%04x  %s%s\n"
                            (mydebug--get item 'number) (mydebug--get item 'address)
                            (or (mydebug--get item 'symbol) "")
                            (if location (format " (%s:%d)" (mydebug--get location 'source)
                                                  (mydebug--get location 'line)) "")))))
        (special-mode)))
    (display-buffer buffer)))

(defun mydebug--handle-state (state)
  (when (mydebug--get state 'registers)
    (mydebug--refresh-registers state)
    (mydebug--show-location (mydebug--get state 'registers)))
  state)

(defun mydebug--command (command &optional args)
  (let ((result (mydebug--request command args)))
    (mydebug--handle-state result)
    result))

(defun mydebug-start (qmp symbols)
  (interactive (list (read-file-name "QMP socket: " nil mydebug-qmp-socket nil)
                     (read-string "Debug map (optional): " mydebug-symbols-file)))
  (setq mydebug-qmp-socket qmp)
  (mydebug--load-map (unless (string-empty-p symbols) symbols))
  (mydebug--ensure-process)
  (mydebug--command "registers")
  (mydebug--refresh-breakpoints)
  (message "MyEmulator debugger connected"))

(defun mydebug-step-instruction () (interactive) (mydebug--command "stepi"))
(defun mydebug-step () (interactive) (mydebug--command "step"))
(defun mydebug-next () (interactive) (mydebug--command "next"))
(defun mydebug-continue () (interactive) (mydebug--command "continue"))
(defun mydebug-interrupt () (interactive) (mydebug--command "stop"))
(defun mydebug-finish () (interactive) (mydebug--command "finish"))
(defun mydebug-reset () (interactive) (mydebug--command "reset"))
(defun mydebug-run () (interactive) (mydebug--command "run"))
(defun mydebug-list () (interactive) (let ((result (mydebug--command "list")))
                                       (with-current-buffer (get-buffer-create "*MyEmulator Source*")
                                         (let ((inhibit-read-only t)) (erase-buffer)
                                           (dolist (item (mydebug--get result 'lines))
                                             (insert (format "%s%4d  %s\n"
                                                             (if (mydebug--get item 'current) "=>" "  ")
                                                             (mydebug--get item 'line)
                                                             (mydebug--get item 'text))))
                                           (special-mode)) (display-buffer (current-buffer)))))

(defun mydebug-toggle-breakpoint ()
  (interactive)
  (let* ((file (buffer-file-name)) (line (line-number-at-pos))
         (address (and file (mydebug--source-address-for-line file line)))
         (existing (seq-find (lambda (item) (= (mydebug--get item 'address) address)) mydebug--breakpoints)))
    (unless address (user-error "No debug-map address for this source line"))
    (if existing (mydebug--command "delete" `((number . ,(mydebug--get existing 'number))))
      (mydebug--command "break" `((address . ,address))))
    (mydebug--refresh-breakpoints)))

(defun mydebug-list-breakpoints () (interactive) (mydebug--refresh-breakpoints))
(defun mydebug-registers () (interactive) (mydebug--command "registers"))
(defun mydebug-print-expression (expression) (interactive "sExpression: ")
  (message "%s" (mydebug--get (mydebug--command "print" `((expression . ,expression))) 'value)))

(defun mydebug-console ()
  (interactive)
  (let ((buffer (get-buffer-create "*MyEmulator Console*")))
    (unless (comint-check-proc buffer)
      (apply #'make-comint-in-buffer "mydebug-console" buffer (mydebug--program)
             nil (append (list "--qmp" mydebug-qmp-socket)
                         (when mydebug--metadata-file (list "--symbols" mydebug--metadata-file)))))
    (pop-to-buffer buffer) (comint-mode)))

(defun mydebug-layout ()
  (interactive)
  (mydebug-registers)
  (mydebug-list-breakpoints)
  (mydebug-console))

(defvar mydebug-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-s") #'mydebug-step-instruction)
    (define-key map (kbd "C-c C-i") #'mydebug-step)
    (define-key map (kbd "C-c C-n") #'mydebug-next)
    (define-key map (kbd "C-c C-c") #'mydebug-continue)
    (define-key map (kbd "C-c C-x") #'mydebug-interrupt)
    (define-key map (kbd "C-c C-f") #'mydebug-finish)
    (define-key map (kbd "C-c C-r") #'mydebug-run)
    (define-key map (kbd "C-c C-0") #'mydebug-reset)
    (define-key map (kbd "C-c C-b") #'mydebug-toggle-breakpoint)
    (define-key map (kbd "C-c C-l") #'mydebug-list)
    (define-key map (kbd "C-c C-p") #'mydebug-print-expression)
    (define-key map (kbd "C-c C-b") #'mydebug-toggle-breakpoint)
    map))

(define-minor-mode mydebug-mode
  "Keymap for a source buffer being debugged by MyEmulator."
  :lighter " MyDbg" :keymap mydebug-mode-map)

(defvar myemulator-asm-font-lock
  `(("^[[:space:]]*\\([[:alnum:]_.$]+\\):" 1 font-lock-function-name-face)
    ("\\_<\\(r[0-3]\\|a[0-3]\\|lr\\|sp\\|pc\\|s0\\)\\_>" . font-lock-variable-name-face)
    ("\\_<\\(li\\|ld\\|st\\|add\\|sub\\|and\\|or\\|xor\\|shl\\|shr\\|cmp\\|jal\\|br\\|beq\\|bne\\|blt\\|bge\\|bltu\\|bgeu\\|mva\\|lda\\|gta\\|ada\\|push\\|pop\\|gf\\|sf\\|ret\\|rti\\|halt\\)\\_>" . font-lock-keyword-face)
    ("^[[:space:]]*\\(\\.[[:alnum:]_]+\\)" 1 font-lock-preprocessor-face)
    (";.*$" . font-lock-comment-face)))

(define-derived-mode myemulator-asm-mode asm-mode "MyEmulator ASM"
  "Explicit syntax mode for MyEmulator assembly; .s is not globally claimed."
  (setq-local font-lock-defaults '(myemulator-asm-font-lock)))

(provide 'mydebug)
;;; mydebug.el ends here
