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

;;; mydebug-tests.el ends here
