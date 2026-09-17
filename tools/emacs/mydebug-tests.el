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

;;; mydebug-tests.el ends here
