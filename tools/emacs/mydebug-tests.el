;;; mydebug-tests.el --- ERT tests for mydebug.el

(require 'ert)
(load-file (expand-file-name "mydebug.el" (file-name-directory (or load-file-name buffer-file-name))))

(ert-deftest mydebug-location-lookup ()
  (let ((mydebug--metadata '((locations . (((address . 4) (source . "foo.s") (line . 7)))))))
    (should (= (mydebug--get (mydebug--location-for-address 4) 'line) 7))
    (should-not (mydebug--location-for-address 6))))

(ert-deftest mydebug-source-line-address ()
  (let ((mydebug--metadata-file "/tmp/program.mdbg")
        (mydebug--metadata
         '((locations . (((address . 4) (source . "foo.s") (line . 7)))))))
    (should (= (mydebug--source-address-for-line "/tmp/foo.s" 7) 4))))

(ert-deftest mydebug-command-bindings ()
  (should (commandp #'mydebug-step-instruction))
  (should (eq (lookup-key mydebug-mode-map (kbd "C-c C-s"))
              #'mydebug-step-instruction)))

(ert-deftest mydebug-asm-mode-is-explicit ()
  (with-temp-buffer
    (myemulator-asm-mode)
    (should (eq major-mode 'myemulator-asm-mode))))

(ert-deftest mydebug-artifact-paths-follow-source ()
  (let ((paths (mydebug--artifact-paths "/tmp/demo/arithmetic.s")))
    (should (equal (plist-get paths :source) "/tmp/demo/arithmetic.s"))
    (should (equal (plist-get paths :binary) "/tmp/demo/arithmetic.bin"))
    (should (equal (plist-get paths :debug-map) "/tmp/demo/arithmetic.debug.json"))))

(ert-deftest mydebug-project-root-is-repository-root ()
  (let ((root (locate-dominating-file default-directory ".git"))
        (source (expand-file-name "examples/asm/arithmetic.asm"
                                 (locate-dominating-file default-directory ".git"))))
    (should (equal (file-truename (mydebug--repository-root source))
                   (file-truename root)))))

(ert-deftest mydebug-qemu-startup-configuration ()
  (let ((root (mydebug--repository-root))
        (command (mydebug--qemu-command
                  (mydebug--repository-root)
                  "/tmp/arithmetic.bin"
                  "/tmp/session/debug.qmp")))
    (should (equal (car command)
                   (expand-file-name ".qemu-build/qemu-system-myemulator" root)))
    (should (member "-S" command))
    (should (member "-kernel" command))
    (should (member "-qmp" command))
    (should (string-match-p "debug.qmp" (car (last command))))))

(defun mydebug-test--state (&optional halted)
  `((registers . ((r0 . 7) (r1 . 0) (r2 . 0) (r3 . 0)
                  (a0 . 0) (a1 . 0) (a2 . 0) (a3 . 0)
                  (lr . 0) (sp . 65535) (pc . 8) (s0 . 0)))
    (flags . ((zf . :false) (nf . :false) (cf . :false)
              (of . :false) (ipl . 0)))
    (halted . ,(if halted t :false))))

(ert-deftest mydebug-console-prompt-and-history ()
  (let (seen)
    (cl-letf (((symbol-function 'mydebug--console-dispatch)
               (lambda (line) (push line seen) "ok")))
      (with-temp-buffer
        (mydebug-console-mode)
        (goto-char (point-max))
        (insert "s")
        (mydebug-console-send-input)
        (mydebug-console-send-input)
        (should (string-match-p "^(mydebug) s" (buffer-string)))
        (should (= (length seen) 2))
        (should (equal seen '("s" "s")))
        (should (lookup-key mydebug-console-mode-map (kbd "M-p")))
        (should (lookup-key mydebug-console-mode-map (kbd "M-n")))))))

(ert-deftest mydebug-console-command-aliases-use-machine-commands ()
  (let (seen)
    (cl-letf (((symbol-function 'mydebug--command)
               (lambda (command &optional _args)
                 (push command seen)
                 (mydebug-test--state))))
      (dolist (line '("s" "si" "n" "fin" "c" "reset" "i r"))
        (mydebug--console-dispatch line))
      (should (equal (nreverse seen)
                     '("step" "stepi" "next" "finish" "continue"
                       "reset" "info registers"))))))

(ert-deftest mydebug-console-identifies-halt-and-refreshes-state ()
  (let ((mydebug--metadata nil))
    (should (string-match-p "HALT at 0x0008"
                            (mydebug--console-format "stepi"
                                                      (mydebug-test--state t))))))

(ert-deftest mydebug-console-keymap-is-session-local ()
  (with-temp-buffer
    (mydebug-mode 1)
    (should (eq (lookup-key mydebug-mode-map (kbd "C-c C-o")) #'mydebug-switch))
    (mydebug-mode -1)
    (should-not (eq (lookup-key (current-local-map) (kbd "C-c C-o"))
                    #'mydebug-switch))))

;;; mydebug-tests.el ends here
