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
  :group 'grimple)

(defcustom grimple-soot-jar
  nil
  "Complete path to Soot 2.5.0 jar for Grimple decompilation."
  :type '(file :must-match t)
  :group 'grimple)

(defcustom grimple-soot-classpath
  nil
  "Extra classpath for Soot Grimple decompilation."
  :type '(set (const 'directory))
  :group 'grimple)

(setq grimple-soot-jar
	  "/home/jbalint/Downloads/soot-2.5.0.jar")

(setq grimple-soot-classpath
	  '("/home/jbalint/sw/stardog-2.1.3/expand"))

(put 'grimple-class-mode 'mode-class 'special)

;; Find the name of the class contained in the given file by executing
;; `javap'. `javap' will not discriminate if the file is not in the
;; correct package as determined by the classpath. In Java 6+ a
;; warning will be printed containing the complete class name +
;; package.
(defun grimple-classname-of-file (file-name)
  "Find the name of the class contained in the given file"
  (let* ((unqual-classname (file-name-base file-name))
		 (command (mapconcat 'identity `("javap" "-cp" ,(file-name-directory file-name) ,unqual-classname) " "))
		 (output (shell-command-to-string command)))
	(if (string-match "contains\\s-+\\([[:alnum:]\\.]+\\)\\b" output)
		(match-string 1 output))))

(defun grimple-package-of-class (classname)
  ""
  (if (string-match "\\(.*\\)\\.[[:alnum:]]+" classname)
	  (match-string 1 classname)))

(defun grimple-decompile-class (classpath-in classname outdir)
  ""
  (let* ((rt-jar (concat (getenv "JAVA_HOME") "/jre/lib/rt.jar"))
		 (classpath (mapconcat 'identity (append `(,rt-jar ,classpath-in) grimple-soot-classpath) ":"))
		 (args `("-jar" ,grimple-soot-jar "-cp" ,classpath "-f" "grimple" "-output-dir" ,outdir ,classname)))
	(apply 'call-process (append '("java" nil "**nil" t) args))))

(defun grimple-replace-buffer-contents-with-decompiled (file-name)
  (let* ((classname (grimple-classname-of-file file-name))
		 (package-dir (replace-regexp-in-string "\\." "/" (grimple-package-of-class classname)))
		 (classpath-for-file (replace-regexp-in-string (concat "/" package-dir "/") "" (file-name-directory file-name)))
		 (grimple-file (concat grimple-tmpdir "/" classname ".grimple")))
	(grimple-decompile-class classpath-for-file classname grimple-tmpdir)
	(erase-buffer)
	(insert-file-contents grimple-file)))

(defun grimple-mode ()
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'grimple-mode)
  (setq mode-name "Grimple")
  (if (string-match "\\.class$" buffer-file-name)
	  (grimple-replace-buffer-contents-with-decompiled buffer-file-name))
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (font-lock-mode nil)
  (turn-on-font-lock))

(add-to-list 'auto-mode-alist '("\\.grimp\\(le\\)?\\'" . grimple-mode))
(add-to-list 'auto-mode-alist '("\\.class\\'" . grimple-mode))

(provide 'grimple-mode)
