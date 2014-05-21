(setq grimple-font-lock-keywords
	  ;; not necessarily exhaustive
	  ;; not sure if while, try, etc are keywords in grimple
	  `((,(concat "\\b" (regexp-opt '("break" "case" "catch" "class" "do" "extends"
									  "final" "finally" "for" "from" "goto" "if"
									  "implements" "import" "new" "package" "private"
									  "protected" "public" "return" "specialinvoke" "static"
									  "staticinvoke" "switch" "synchronized" "throw"
									  "throws" "to" "transient" "try" "virtualinvoke" "with"
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

(defcustom grimple-decompile-format
  ;;"grimp"
  "jimple"
  "Soot format used to de/re-compile code."
  :type 'string
  :group 'grimple)

(setq grimple-soot-jar
	  "/home/jbalint/Downloads/soot-2.5.0.jar")

(setq grimple-soot-classpath nil)
	  ;;'("/home/jbalint/sw/stardog-2.1.3/expand"))

(put 'grimple-class-mode 'mode-class 'special)

;; Find the name of the class contained in the given file by executing
;; `javap'. `javap' will not discriminate if the file is not in the
;; correct package as determined by the classpath. In Java 6+ a
;; warning will be printed containing the complete class name +
;; package.
(defun grimple-classname-of-file (file-name)
  "Find the name of the class contained in the given file"
  (let* ((unqual-classname (file-name-base file-name))
		 (command (mapconcat 'identity
							 `("javap" "-cp" ,(file-name-directory file-name)
							   ,unqual-classname) " "))
		 (output (shell-command-to-string command)))
	(if (string-match "contains\\s-+\\([[:alnum:]\\.]+\\)\\b" output)
		(match-string 1 output))))

(defun grimple-rt-jar-path ()
  ""
  (concat (getenv "JAVA_HOME") "/jre/lib/rt.jar"))

(defun grimple-base-classpath (&rest extras)
  ""
  (mapconcat 'identity (append (list (grimple-rt-jar-path) grimple-tmpdir) grimple-soot-classpath extras) ":"))

(defun grimple-package-of-class (classname)
  "Separate the package from the given fully qualified classname."
  (if (string-match "\\(.*\\)\\.[[:alnum:]]+" classname)
	  (match-string 1 classname)))

(defun grimple-build-args (extra-classpath &rest inargs)
  ""
  (let ((args (or inargs '())))
	(append `("-jar" ,grimple-soot-jar "-cp" ,(grimple-base-classpath extra-classpath)) args)))

(defun grimple-soot-classpath-add-jardir (dir)
  "Add a directory full of jars to `grimple-soot-classpath'."
  (interactive)
  (setq grimple-soot-classpath
		(append grimple-soot-classpath
				(directory-files dir t "jar$"))))

(defun grimple-decompile-class (classpath-in classname outdir)
  "Decompile."
  (let ((args (grimple-build-args classpath-in "-f" grimple-decompile-format
								  "-output-dir" outdir classname))
		(buf (get-buffer-create "*soot-decompile*")))
	(with-current-buffer buf
	  (insert (format "\n\njava soot-decompile args: %s" args)))
	;; TODO would be nice to use the buf variable here instead of the buffer name
	(unless (= 0 (apply 'call-process (append '("java" nil "*soot-decompile*" t) args)))
	  (switch-to-buffer buf)
	  (error "Cannot decompile"))))

(defun grimple-save-class ()
  "Save (recompile) a class."
  (let ((args (grimple-build-args nil "-src-prec" "J" "-output-dir" grimple-base-path grimple-classname))
		(buf (get-buffer-create "*soot-recompile*")))
	(write-region nil nil grimple-decompiled-file)
	(with-current-buffer buf
	  (insert (format "\n\njava soot-recompile args: %s\n" args)))
	(unless (= 0 (apply 'call-process (append '("java" nil "*soot-recompile*" t) args)))
	  (switch-to-buffer buf)
	  (error "Cannot save"))))

(defun grimple-replace-buffer-contents-with-decompiled ()
  ""
  (grimple-decompile-class grimple-base-path grimple-classname grimple-tmpdir)
  (erase-buffer)
  (insert-file-contents grimple-decompiled-file)
  (set-buffer-modified-p nil))

(defun grimple-setup-class-mode ()
  ""
  (set (make-local-variable 'grimple-classname)
	   (grimple-classname-of-file buffer-file-name))
  (set (make-local-variable 'grimple-base-path)
	   (let ((package-dir (replace-regexp-in-string "\\." "/" (grimple-package-of-class grimple-classname))))
		 (replace-regexp-in-string (concat "/" package-dir "/") "" (file-name-directory buffer-file-name))))
  (set (make-local-variable 'grimple-decompiled-file)
	   (concat grimple-tmpdir "/" grimple-classname "." grimple-decompile-format))
  (set (make-local-variable 'write-file-functions)
	   'grimple-save-class)
  (grimple-replace-buffer-contents-with-decompiled))

(defun grimple-mode ()
  ""
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'grimple-mode)
  (setq mode-name "Grimple")
  (if (string-match "\\.class$" buffer-file-name)
	  (grimple-setup-class-mode))
  (auto-save-mode 0)
										;(setq file-precious-flag t)
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (font-lock-mode nil)
  (turn-on-font-lock))

(add-to-list 'auto-mode-alist '("\\.grimp\\(le\\)?\\'" . grimple-mode))
(add-to-list 'auto-mode-alist '("\\.class\\'" . grimple-mode))

(provide 'grimple-mode)
