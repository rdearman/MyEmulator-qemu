;;; mydebug.el --- MyEmulator native debugger integration -*- lexical-binding: t; -*-

;; This package talks only to tools/mydebug --machine.  The interactive
;; mydebug console remains available in a separate comint buffer.

(require 'asm-mode)
(require 'cl-lib)
(require 'comint)
(require 'json)
(require 'seq)

(defgroup mydebug nil "MyEmulator native debugger." :group 'tools)
(defcustom mydebug-program nil "Path to tools/mydebug." :type '(choice (const nil) file))
(defcustom mydebug-assembler-program nil "Assembler command, or nil to use the project assembler." :type '(choice (const nil) file))
(defcustom mydebug-qemu-program nil "QEMU command, or nil to use the project build." :type '(choice (const nil) file))
(defcustom mydebug-qmp-socket nil
  "Configured QMP socket for explicit attach, or nil for an automatic session."
  :type '(choice (const nil) file))
(defcustom mydebug-symbols-file nil "Default MyEmulator debug-map file." :type '(choice (const nil) file))
(defcustom mydebug-auto-start-qemu t "Whether `mydebug-start' starts the project QEMU automatically." :type 'boolean)
(defcustom mydebug-image-type 'auto
  "Image type for automatic sessions: `auto', `ram', or `firmware'."
  :type '(choice (const auto) (const ram) (const firmware)))
(defcustom mydebug-source-face 'highlight "Face for the current guest source line." :type 'face)

(defconst mydebug--directory
  (file-name-directory (or load-file-name buffer-file-name)))

(defvar mydebug--process nil)
(defvar mydebug--buffer "*MyEmulator Debugger*")
(defvar mydebug--responses nil)
(defvar mydebug--partial "")
(defvar mydebug--metadata nil)
(defvar mydebug--metadata-file nil)
(defvar mydebug--source-file nil)
(defvar mydebug--binary-file nil)
(defvar mydebug--qemu-process nil)
(defvar mydebug--qemu-temp-dir nil)
(defvar mydebug--qemu-owned nil)
(defvar mydebug--session-qmp-socket nil)
(defvar mydebug--pc-overlay nil)
(defvar mydebug--breakpoint-overlays nil)
(defvar mydebug--previous-registers nil)
(defvar mydebug--breakpoints nil)
(defvar mydebug--console-last-command nil)
(defvar mydebug--console-running nil)

(defun mydebug--program ()
  (or mydebug-program
      (expand-file-name "../mydebug" mydebug--directory)))

(defun mydebug--repository-root (&optional file)
  "Find the MyEmulator project root for FILE or the current buffer."
  (let* ((name (or file buffer-file-name default-directory))
         (directory (file-name-directory (expand-file-name name))))
    (or (locate-dominating-file directory "tools/myasm")
        (locate-dominating-file directory ".git")
        (expand-file-name "../.." mydebug--directory))))

(defun mydebug--source (&optional required)
  (or buffer-file-name
      (and required (user-error "Save the assembly buffer before starting MyEmulator"))))

(defun mydebug--artifact-paths (&optional source)
  "Return plist paths for SOURCE's binary and JSON debug map."
  (let* ((source (or source (mydebug--source t)))
         (stem (file-name-sans-extension (expand-file-name source))))
    (list :source (expand-file-name source)
          :binary (concat stem ".bin")
          :debug-map (concat stem ".debug.json"))))

(defun mydebug--source-image-type (source)
  "Return the image type for SOURCE, honoring `mydebug-image-type'."
  (if (not (eq mydebug-image-type 'auto))
      mydebug-image-type
    (with-temp-buffer
      (insert-file-contents source)
      (if (re-search-forward
           "^[[:space:]]*\\.org[[:space:]]+\\(?:0[xX]\\)?[fF]100\\b" nil t)
          'firmware
        'ram))))

(defun mydebug--assembler-command (root)
  (let ((wrapper (or mydebug-assembler-program
                     (expand-file-name "tools/myasm" root))))
    (if (file-executable-p wrapper)
        (list wrapper)
      (let ((python (or (executable-find "python3") (executable-find "python")))
            (script (expand-file-name "tools/assembler/myasm.py" root)))
        (unless (and python (file-exists-p script))
          (user-error "Cannot find the MyEmulator assembler under %s" root))
        (list python script)))))

(defun mydebug--assemble (source binary debug-map &optional image-type)
  "Assemble SOURCE into BINARY and DEBUG-MAP, showing failures in a buffer."
  (let* ((root (mydebug--repository-root source))
         (command (mydebug--assembler-command root))
         (buffer (get-buffer-create "*MyEmulator Build*"))
         (program (car command))
         (args (append (cdr command) (list source "-o" binary "--debug-map" debug-map)
                       (when (eq image-type 'firmware) (list "--firmware")))))
    (with-current-buffer buffer
      (let ((inhibit-read-only t))
        (erase-buffer)
        (insert (format "$ %s\n\n" (mapconcat #'identity (cons program args) " ")))))
    (let ((status (apply #'call-process program nil buffer t args)))
      (with-current-buffer buffer (compilation-mode))
      (unless (and (integerp status) (= status 0))
        (display-buffer buffer)
        (user-error "Assembly failed; see *MyEmulator Build*"))
      status)))

(defun mydebug--ensure-artifacts (source)
  (let* ((paths (mydebug--artifact-paths source))
         (image-type (mydebug--source-image-type source))
         (binary (plist-get paths :binary))
         (debug-map (plist-get paths :debug-map)))
    (when (or (not (file-exists-p binary))
              (not (file-exists-p debug-map))
              (file-newer-than-file-p source binary)
              (file-newer-than-file-p source debug-map))
      (message "Assembling %s" (file-name-nondirectory source))
      (mydebug--assemble source binary debug-map image-type))
    (setq paths (plist-put paths :image-type image-type))
    paths))

(defun mydebug-build ()
  "Assemble the current MyEmulator source and generate its debug map."
  (interactive)
  (let* ((source (mydebug--source t))
         (paths (mydebug--artifact-paths source))
         (image-type (mydebug--source-image-type source)))
    (mydebug--assemble source (plist-get paths :binary) (plist-get paths :debug-map)
                        image-type)
    (message "Built %s and %s" (file-name-nondirectory (plist-get paths :binary))
             (file-name-nondirectory (plist-get paths :debug-map)))))

(defun mydebug--qemu-command (root binary socket &optional image-type)
  (let ((qemu (or mydebug-qemu-program
                  (expand-file-name ".qemu-build/qemu-system-myemulator" root)
                  (executable-find "qemu-system-myemulator"))))
    (unless (and qemu (file-executable-p qemu))
      (user-error "Cannot find qemu-system-myemulator; build .qemu-build first"))
    (list qemu "-M" "myemulator" "-S" "-display" "none" "-serial" "none"
          (if (eq image-type 'firmware) "-bios" "-kernel") binary
          "-qmp" (concat "unix:" socket ",server=on,wait=off"))))

(defun mydebug--active-qmp-socket ()
  (or mydebug--session-qmp-socket mydebug-qmp-socket))

(defun mydebug--start-qemu (root binary &optional image-type)
  (unless mydebug-auto-start-qemu
    (user-error "Automatic QEMU startup is disabled; use `mydebug-start-attach'"))
  (setq mydebug--qemu-temp-dir (make-temp-file "myemulator-qmp-" t)
        mydebug--session-qmp-socket (expand-file-name "debug.qmp" mydebug--qemu-temp-dir)
        mydebug--qemu-owned t)
  (let ((command (mydebug--qemu-command root binary mydebug--session-qmp-socket image-type)))
    (setq mydebug--qemu-process
          (apply #'start-process "myemulator-qemu" "*MyEmulator QEMU*" command))
    (set-process-query-on-exit-flag mydebug--qemu-process nil)
    (let ((deadline (+ (float-time) 5.0)))
      (while (and (not (file-exists-p mydebug--session-qmp-socket))
                  (process-live-p mydebug--qemu-process)
                  (< (float-time) deadline))
        (accept-process-output mydebug--qemu-process 0.05)))
    (unless (file-exists-p mydebug--session-qmp-socket)
      (mydebug--shutdown-session)
      (user-error "QEMU did not create its debugger socket"))))

(defun mydebug--shutdown-session ()
  (when (process-live-p mydebug--process) (delete-process mydebug--process))
  (setq mydebug--process nil mydebug--responses nil mydebug--partial "")
  (when (and mydebug--qemu-owned (process-live-p mydebug--qemu-process))
    (delete-process mydebug--qemu-process))
  (when (and mydebug--qemu-temp-dir (file-directory-p mydebug--qemu-temp-dir))
    (delete-directory mydebug--qemu-temp-dir t))
  (setq mydebug--qemu-process nil mydebug--qemu-temp-dir nil
        mydebug--qemu-owned nil mydebug--session-qmp-socket nil))

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

(defun mydebug--send-request (command &optional args)
  (let ((process (mydebug--ensure-process)))
    (process-send-string process
                         (concat (json-encode `((command . ,command)
                                                (args . ,(or args (make-hash-table))))) "\n"))))

(defun mydebug--ensure-process ()
  (unless (process-live-p mydebug--process)
    (setq mydebug--responses nil mydebug--partial "")
    (let ((socket (mydebug--active-qmp-socket)))
      (unless (and socket (file-exists-p socket))
        (user-error "QMP socket is unavailable: %s" (or socket "none")))
      (let ((args (list "--machine" "--qmp" socket)))
      (when mydebug--metadata-file (setq args (append args (list "--symbols" mydebug--metadata-file))))
      (setq mydebug--process
            (make-process :name "mydebug-machine" :buffer mydebug--buffer
                          :command (cons (mydebug--program) args)
                          :connection-type 'pipe :noquery t
                          :filter #'mydebug--filter
                          :sentinel (lambda (p event)
                                      (unless (process-live-p p)
                                        (message "mydebug: backend exited (%s)" (string-trim event)))))))))
  mydebug--process)

(defun mydebug--request (command &optional args)
  (let ((process (mydebug--ensure-process)))
    (mydebug--send-request command args)
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
              (display-buffer buffer))))))))

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
        (when (mydebug--bool (mydebug--get state 'halted))
          (insert "\nStopped: HALT\n"))
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

(defun mydebug--location-text (state)
  (let* ((registers (mydebug--get state 'registers))
         (pc (and registers (mydebug--get registers 'pc)))
         (location (and pc (mydebug--location-for-address pc))))
    (concat (if (mydebug--bool (mydebug--get state 'halted)) "HALT at " "")
            (if location
                (format "0x%04x %s:%d" pc (mydebug--get location 'source)
                        (mydebug--get location 'line))
              (format "0x%04x" (or pc 0))))))

(defun mydebug--format-registers (state)
  (let ((registers (mydebug--get state 'registers))
        (flags (mydebug--get state 'flags)))
    (concat
     (mapconcat
      (lambda (name)
        (let ((value (mydebug--get registers name)))
          (format "%-3s  %s" (upcase (symbol-name name))
                  (if (or (string-prefix-p "r" (symbol-name name))
                          (eq name 's0))
                      (format "0x%02x" value)
                    (format "0x%04x" value)))))
      '(r0 r1 r2 r3 a0 a1 a2 a3 lr sp pc s0) "\n")
     (format "\nZF=%d NF=%d CF=%d OF=%d IPL=%d"
             (if (mydebug--bool (mydebug--get flags 'zf)) 1 0)
             (if (mydebug--bool (mydebug--get flags 'nf)) 1 0)
             (if (mydebug--bool (mydebug--get flags 'cf)) 1 0)
             (if (mydebug--bool (mydebug--get flags 'of)) 1 0)
             (mydebug--get flags 'ipl)))))

(defun mydebug--console-help (&optional command)
  (let ((help '("s/step       source-line step"
                "si/stepi     one-instruction step"
                "n/next       step over a call"
                "c/continue   continue execution"
                "fin/finish   run to LR return"
                "r/run        rebuild, restart and run"
                "reset        reset and stop"
                "b/break A    set breakpoint"
                "d/delete ID  delete breakpoint"
                "bt           backtrace"
                "l/list       list source"
                "p EXPR       print expression"
                "set R = E    set register"
                "i r|b|l      info registers/breakpoints/locals"
                "?            show this help")))
    (if command
        (or (seq-find (lambda (line) (string-prefix-p command line)) help)
            (format "No help for %s" command))
      (mapconcat #'identity help "\n"))))

(defun mydebug--console-format (command result)
  (cond
   ((member command '("?" "help")) result)
   ((member command '("regs" "registers" "info registers" "i r"))
    (mydebug--format-registers result))
   ((member command '("s" "step" "si" "stepi" "n" "next" "fin" "finish"))
    (format "stopped at %s" (mydebug--location-text result)))
   ((member command '("c" "continue" "reset" "run" "r" "stop"))
    (format "stopped at %s" (mydebug--location-text result)))
   ((member command '("b" "break"))
    (format "Breakpoint %d at 0x%04x%s"
            (mydebug--get result 'number) (mydebug--get result 'address)
            (if (mydebug--get result 'location)
                (format " (%s:%d)" (mydebug--get (mydebug--get result 'location) 'source)
                        (mydebug--get (mydebug--get result 'location) 'line)) "")))
   ((member command '("d" "delete"))
    (format "Deleted %s" (if (stringp (mydebug--get result 'deleted))
                              "all breakpoints"
                            (mydebug--get (mydebug--get result 'deleted) 'number))))
   ((member command '("i b" "info breakpoints" "breakpoints"))
    (if (null (mydebug--get result 'breakpoints))
        "No breakpoints."
      (concat "Num  Address  Location\n"
              (mapconcat
               (lambda (item)
                 (let ((location (mydebug--get item 'location)))
                   (format "%-4d 0x%04x  %s%s" (mydebug--get item 'number)
                           (mydebug--get item 'address)
                           (or (mydebug--get item 'symbol) "")
                           (if location (format " (%s:%d)" (mydebug--get location 'source)
                                                 (mydebug--get location 'line)) ""))))
               (mydebug--get result 'breakpoints) "\n"))))
   ((member command '("i l" "info locals")) (or (mydebug--get result 'message) "No locals."))
   ((member command '("l" "list"))
    (if (mydebug--bool (mydebug--get result 'available))
        (mapconcat (lambda (item)
                     (format "%s %4d  %s" (if (mydebug--get item 'current) "=>" "  ")
                             (mydebug--get item 'line) (mydebug--get item 'text)))
                   (mydebug--get result 'lines) "\n")
      (or (mydebug--get result 'message) "No source mapping.")))
   ((member command '("p" "print"))
    (let ((value (mydebug--get result 'value)))
      (format "0x%x" value)))
   ((member command '("x" "examine"))
    (format "%04x: %s" (mydebug--get result 'address) (mydebug--get result 'data)))
   ((member command '("dis" "disassemble"))
    (mapconcat (lambda (item) (format "%04x: %s" (mydebug--get item 'address)
                                      (mydebug--get item 'text)))
               (mydebug--get result 'instructions) "\n"))
   ((member command '("bt" "backtrace"))
    (mapconcat (lambda (frame)
                 (format "#%d %s" (mydebug--get frame 'level)
                         (mydebug--location-text `((registers . ((pc . ,(mydebug--get frame 'address))))))))
               (mydebug--get result 'frames) "\n"))
   ((member command '("set")) (mydebug--format-registers result))
   (t (format "%S" result))))

(defun mydebug--console-dispatch (line)
  "Dispatch a human console LINE through the structured machine protocol."
  (let* ((line (string-trim line))
         (parts (split-string line "[ \t]+" t))
         (command (downcase (or (car parts) "")))
         (rest (string-trim (substring line (min (length line)
                                                 (length (or (car parts) "")))))))
    (cond
     ((or (string-empty-p line) (member command '("?" "help")))
      (mydebug--console-help (and (string= command "help") rest)))
     ((member command '("i" "info"))
      (let ((topic (downcase (or (car (split-string rest "[ \t]+" t)) ""))))
        (unless (member topic '("r" "registers" "b" "breakpoints" "l" "locals"))
          (user-error "Use: i r, i b, or i l"))
        (let ((full (cond ((member topic '("r" "registers")) "info registers")
                          ((member topic '("b" "breakpoints")) "info breakpoints")
                          (t "info locals"))))
          (mydebug--console-format full (mydebug--command full)))))
     ((member command '("regs" "registers"))
      (mydebug--console-format command (mydebug--command "registers")))
     ((member command '("s" "step" "si" "stepi" "n" "next" "fin" "finish" "c" "continue" "stop" "reset"))
      (let ((result (pcase command
                      ((or "s" "step") (mydebug--command "step"))
                      ((or "si" "stepi") (mydebug--command "stepi"))
                      ((or "n" "next") (mydebug--command "next"))
                      ((or "fin" "finish") (mydebug--command "finish"))
                      ((or "c" "continue") (mydebug--command "continue"))
                      ("stop" (mydebug--command "stop"))
                      ("reset" (mydebug--command "reset")))))
        (mydebug--console-format command result)))
     ((member command '("r" "run"))
      (when (not (string-empty-p rest)) (user-error "run does not accept arguments"))
      (mydebug--console-format "run" (mydebug-run)))
     ((member command '("b" "break"))
      (when (string-empty-p rest) (user-error "break requires an address or symbol"))
      (let ((result (mydebug--command "break" `((address . ,rest)))))
        (mydebug--refresh-breakpoints)
        (mydebug--console-format command result)))
     ((member command '("d" "delete"))
      (if (string-empty-p rest)
          (if (y-or-n-p "Delete all MyEmulator breakpoints? ")
              (let ((result (mydebug--command "delete")))
                (mydebug--refresh-breakpoints)
                (mydebug--console-format command result))
            "Delete cancelled")
        (let ((result (mydebug--command "delete" `((number . ,(string-to-number rest))))))
          (mydebug--refresh-breakpoints)
          (mydebug--console-format command result))))
     ((member command '("p" "print"))
      (let ((format "x") (expression rest)))
        (when (string-match "^/\([xdot]\)[ \t]+\(.+\)$" rest)
          (setq format (match-string 1 rest) expression (match-string 2 rest)))
        (let ((result (mydebug--command "print" `((expression . ,expression)
                                                   (format . ,format))))
          (format (pcase format
                    ("d" "%d") ("o" "0o%o") ("t" "0b%b") (_ "0x%x"))
                  (mydebug--get result 'value)))))
     ((string= command "set")
      (unless (string-match "^\([[:alnum:]_]+\)[ \t]*=[ \t]*\(.+\)$" rest)
        (user-error "Use: set REGISTER = EXPRESSION"))
      (mydebug--console-format command
                                (mydebug--command "set"
                                                  `((register . ,(match-string 1 rest))
                                                    (expression . ,(match-string 2 rest))))))
     ((member command '("l" "list"))
      (mydebug--console-format command
                               (mydebug--command "list"
                                                 (unless (string-empty-p rest)
                                                   `((target . ,rest))))))
     ((member command '("bt" "backtrace"))
      (mydebug--console-format command (mydebug--command "backtrace")))
     ((member command '("x" "examine"))
      (let ((args (split-string rest "[ \t]+" t)))
        (unless (car args) (user-error "Use: x ADDRESS [LENGTH]"))
        (mydebug--console-format command
                                 (mydebug--command "x"
                                                   `((address . ,(car args))
                                                     (length . ,(string-to-number (or (cadr args) "16"))))))))
     ((member command '("dis" "disassemble"))
      (let ((args (split-string rest "[ \t]+" t)))
        (unless (car args) (user-error "Use: dis ADDRESS [COUNT]"))
        (mydebug--console-format command
                                 (mydebug--command "dis"
                                                   `((address . ,(car args))
                                                     (count . ,(string-to-number (or (cadr args) "8"))))))))
     (t (user-error "Unknown command; type ? for help")))))

(defun mydebug--command (command &optional args)
  (let ((result (mydebug--request command args)))
    (mydebug--handle-state result)
    result))

(defun mydebug--start-session (source paths &optional attach)
  (setq mydebug--source-file source
        mydebug--binary-file (plist-get paths :binary))
  (mydebug--load-map (plist-get paths :debug-map))
  (unless attach
    (mydebug--start-qemu (mydebug--repository-root source) mydebug--binary-file
                          (plist-get paths :image-type)))
  (mydebug--ensure-process)
  (with-current-buffer (find-file-noselect source) (mydebug-mode 1))
  (mydebug--command "registers")
  (mydebug--refresh-breakpoints)
  (message "MyEmulator debugger connected: %s" (file-name-nondirectory source)))

(defun mydebug-start ()
  "Build, launch and attach to MyEmulator for the current source buffer."
  (interactive)
  (let* ((source (mydebug--source t))
         (paths (mydebug--ensure-artifacts source)))
    (when (process-live-p mydebug--process)
      (mydebug--command "registers")
      (message "MyEmulator debugger already connected")
      (cl-return-from mydebug-start))
    (when mydebug-qmp-socket
      (unless (file-exists-p mydebug-qmp-socket)
        (user-error "Configured QMP socket is missing: %s" mydebug-qmp-socket))
      (setq mydebug--session-qmp-socket mydebug-qmp-socket)
      (mydebug--start-session source paths t)
      (cl-return-from mydebug-start))
    (mydebug--start-session source paths)))

(defun mydebug-start-attach (qmp symbols)
  "Attach to an already-running QEMU using explicitly selected files."
  (interactive
   (list (read-file-name "QMP socket: " nil nil t)
         (read-file-name "Debug map: "
                         (file-name-directory (or buffer-file-name default-directory))
                         nil t)))
  (let ((source (mydebug--source t)))
    (setq mydebug-qmp-socket (expand-file-name qmp))
    (unless (file-exists-p symbols)
      (user-error "Debug map is missing: %s" symbols))
    (setq mydebug--session-qmp-socket mydebug-qmp-socket)
    (mydebug--start-session source (list :source source :binary nil
                                         :debug-map (expand-file-name symbols)) t)))

(defun mydebug-step-instruction () (interactive) (mydebug--command "stepi"))
(defun mydebug-step () (interactive) (mydebug--command "step"))
(defun mydebug-next () (interactive) (mydebug--command "next"))
(defun mydebug-continue () (interactive) (mydebug--command "continue"))
(defun mydebug-interrupt () (interactive) (mydebug--command "stop"))
(defun mydebug-finish () (interactive) (mydebug--command "finish"))
(defun mydebug-reset () (interactive) (mydebug--command "reset"))
(defun mydebug-run ()
  "Rebuild if needed, restart the owned QEMU session, and run from reset."
  (interactive)
  (let* ((source (or mydebug--source-file (mydebug--source t)))
         (paths (mydebug--ensure-artifacts source)))
    (unless mydebug--qemu-owned
      (user-error "Cannot reload an attached QEMU; use `mydebug-start' with an owned session"))
    (mydebug--shutdown-session)
    (mydebug--start-session source paths)
    (mydebug--command "run")))
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

(defvar mydebug-console-mode-map
  (let ((map (make-sparse-keymap)))
    (set-keymap-parent map comint-mode-map)
    (define-key map (kbd "RET") #'mydebug-console-send-input)
    (define-key map (kbd "C-m") #'mydebug-console-send-input)
    (define-key map (kbd "C-c C-c") #'mydebug-console-interrupt)
    (define-key map (kbd "C-c C-x") #'mydebug-console-interrupt)
    (define-key map (kbd "C-c C-o") #'mydebug-switch)
    (define-key map (kbd "<f5>") #'mydebug-continue)
    (define-key map (kbd "<f10>") #'mydebug-next)
    (define-key map (kbd "<f11>") #'mydebug-step)
    (define-key map (kbd "S-<f11>") #'mydebug-finish)
    map))

(define-derived-mode mydebug-console-mode comint-mode "MyDebug"
  "GDB-like command transcript for the structured MyEmulator debugger."
  (setq-local comint-prompt-regexp "^(mydebug) ")
  (setq-local comint-prompt-read-only t)
  (setq-local comint-input-ignoredups t)
  (setq-local comint-input-ring (make-ring comint-input-ring-size))
  (let ((inhibit-read-only t))
    (erase-buffer)
    (insert (propertize "(mydebug) " 'read-only t 'rear-nonsticky '(read-only))))
  (setq-local comint-last-input-end (copy-marker (point))))

(defun mydebug-console--insert-prompt ()
  (let ((inhibit-read-only t))
    (goto-char (point-max))
    (insert (propertize "(mydebug) " 'read-only t 'rear-nonsticky '(read-only)))
    (setq comint-last-input-end (copy-marker (point)))))

(defun mydebug-console-send-input ()
  "Execute the current console line through mydebug's machine protocol."
  (interactive)
  (let* ((start (or comint-last-input-end (point-min)))
         (input (string-trim (buffer-substring-no-properties start (point-max)))))
    (when (string-empty-p input)
      (setq input mydebug--console-last-command)
      (unless input (user-error "No previous debugger command"))
      (let ((inhibit-read-only t)) (goto-char (point-max)) (insert input)))
    (setq mydebug--console-last-command input)
    (comint-add-to-input-history input)
    (let ((inhibit-read-only t)
          (input-end (point-max))
          output)
      (add-text-properties start input-end '(read-only t))
      (goto-char input-end)
      (insert "\n")
      (condition-case err
          (setq output (mydebug--console-dispatch input))
        (error (setq output (format "error: %s" (error-message-string err)))))
      (insert (or output ""))
      (insert "\n")
      (mydebug-console--insert-prompt)
      (goto-char (point-max)))))

(defun mydebug-console-interrupt ()
  "Stop the guest without killing the debugger command buffer."
  (interactive)
  (let ((output (condition-case err
                    (mydebug--console-format "stop" (mydebug--command "stop"))
                  (error (format "error: %s" (error-message-string err))))))
    (let ((inhibit-read-only t))
      (goto-char (point-max))
      (insert "^C\n" output "\n")
      (mydebug-console--insert-prompt))))

(defun mydebug-console ()
  (interactive)
  (let ((buffer (get-buffer-create "*MyEmulator Console*")))
    (unless (derived-mode-p 'mydebug-console-mode)
      (with-current-buffer buffer (mydebug-console-mode)))
    (pop-to-buffer buffer)))

(defun mydebug-switch ()
  "Switch between the current source buffer and the MyEmulator console."
  (interactive)
  (if (derived-mode-p 'mydebug-console-mode)
      (if (buffer-live-p (get-file-buffer mydebug--source-file))
          (pop-to-buffer (get-file-buffer mydebug--source-file))
        (mydebug-console))
    (mydebug-console)))

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
    (define-key map (kbd "C-c C-o") #'mydebug-switch)
    (define-key map (kbd "C-x SPC") #'mydebug-toggle-breakpoint)
    (define-key map (kbd "<f5>") #'mydebug-continue)
    (define-key map (kbd "<f10>") #'mydebug-next)
    (define-key map (kbd "<f11>") #'mydebug-step)
    (define-key map (kbd "S-<f11>") #'mydebug-finish)
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
