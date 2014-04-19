;; This buffer is for notes you don't want to save, and for Lisp evaluation.
;; If you want to create a file, visit that file with C-x C-f,
;; then enter the text in that file's own buffer.



(setq grimple-font-lock-keywords
	  ;; not necessarily exhaustive
	  ;; not sure if while, try, etc are keywords in grimple
	  `((,(concat "\\b" (regexp-opt '("break" "case" "catch" "class" "do" "extends"
									  "final" "finally" "for" "from" "goto" "if"
									  "implements" "import" "new" "package" "private"
									  "protected" "public" "return" "static"
									  "staticinvoke" "switch" "synchronized" "throw"
									  "throws" "to" "transient" "try" "with"
									  "while")) "\\b")
		 . font-lock-function-name-face)
		;; TODO labels, strings
		(,(concat "\\b" (regexp-opt '("boolean" "char" "double" "float" "int" "long"
									  "short" "void")) "\\b")
		 . font-lock-type-face)
		("\\b\\(null\\)\\b" . font-lock-constant-face)
		("\\blabel[[:digit:]]+\\b" . font-lock-constant-face)))

;; copied from archive-tmpdir (arc-mode.el)
(defcustom grimple-tmpdir
  ;; make-temp-name is safe here because we use this name
  ;; to create a directory.
  (make-temp-name
   (expand-file-name ;;(if (eq system-type 'ms-dos) "ar" "archive.tmp")
		     temporary-file-directory))
  "Directory for temporary files made by `grimple-mode.el'."
  :type 'directory
  :group 'archive)

(put 'grimple-class-mode 'mode-class 'special)

(defun grimple-class-name-of-file-process-filter (proc output)
  ;; TODO does not handle chunking properly
  ;; TODO does not handle Java 5 version of javap which doesn't give warning
  (if (string-match "contains\\s-+\\([[:alnum:]\\.]+\\)\\b" output)
	  (match-string 1 output)))

;; Find the name of the class contained in the given file by executing
;; `javap'. `javap' will not discriminate if the file is not in the
;; correct package as determined by the classpath. In Java 6+ a
;; warning will be printed containing the complete class name +
;; package.
(defun grimple-class-name-of-file (file-name)
  "Find the name of the class contained in the given file"
  (let ((unqual-class-name (file-name-base file-name)))
	(set-process-filter
	 (start-process "grimple-class-finder" nil "javap" "-cp" (file-name-directory file-name) unqual-class-name)
	 'grimple-class-name-of-file-process-filter)))

(defun grimple-decompile-class (file-name)
  (let* ((class-name (replace-regexp-in-string "pattern" "replacement" file-name))
		  (args '("-jar" grimple-soot-jar "-cp" "CLASSPATH" "-f" "grimple" class-name)))
	(call-process "java" nil nil nil args)))

(defun grimple-class-mode ()
  (grimple-mode)
  (kill-all-local-variables)
  (setq major-mode 'grimple-class-mode)
  (setq mode-name "Grimple")
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (grimple-decompile-class buffer-file-name)
  (font-lock-mode nil)
  (turn-on-font-lock))

(defun grimple-mode ()
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'grimple-mode)
  (setq mode-name "Grimple")
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (font-lock-mode nil)
  (turn-on-font-lock))

(add-to-list 'auto-mode-alist '("\\.grimp\\(le\\)?\\'" . grimple-mode))
(add-to-list 'auto-mode-alist '("\\.class\\'" . grimple-class-mode))

(provide 'grimple-mode)
