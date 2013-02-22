;;; yt.el --- GUD Mode for YellowTree debugger
;; requires "semantic" tool (optional?)
;; Write me some more docs

(require 'gud)

;; map of primitive names to Java internal names
(setq java-primitive-types
      '(("boolean" . "Z")
	("byte"    . "B")
	("char"    . "C")
	("short"   . "S")
	("int"     . "I")
	("long"    . "J")
	("float"   . "F")
	("double"  . "D")
	("void"    . "V")))

;; Convert a class name to internal form
;;
;; java.lang.String -> java/lang/String
;; handles inner classes (with capital names)
;; x.A.A -> x/A$A
(defun java-class-name-to-internal-name (class)
  "Convert a full Java class name to the internal format.
   This follows the official Java class and package naming
   conventions. Class names and inner classes, enums, etc
   should begin with capital letters and all components of
   package names should begin with lower letters."
  ;; Add an override to allow all-caps components of pkg names?
  ;; There's odd stuff out there like EDU.oswego.cs.dl.util.concurrent
  (let ((pieces (split-string class "\\."))
	(case-fold-search nil)
	(sep "/")
	name
	p)
    (while pieces
      (setq p (car pieces))
      (setq pieces (cdr pieces))
      (setq name (concat name p))
      (if (string-match "^[A-Z]" p) (setq sep "$"))
      (let ((next (car pieces)))
	(if next (setq name (concat name sep)))))
    name))

;; Encode a Java type to internal form
(defun encode-java-type (type)
  (let* ((native-type-code (cdr (assoc type java-primitive-types)))
	 (internal-name (if (and type (not native-type-code))
			    (java-class-name-to-internal-name type))))
    (or native-type-code
	(concat "L" internal-name ";"))))

(defun current-internal-method-signature ()
  "Get the signature for the current method in internal form"
  (let ((tag (semantic-current-tag)))
    (if (semantic-tag-of-class-p tag 'function)
	(let* ((method (semantic-tag-name tag))
	       (class (subst-char-in-string ?. ?/ (current-java-class)))
	       (rettype (encode-java-type (semantic-tag-type tag)))
	       (args (mapconcat (lambda (tag) (encode-java-type (semantic-tag-type tag)))
				(semantic-tag-function-arguments tag) ""))
	       (sig (concat class "." method "(" args ")" rettype)))
	  sig))))

(defun current-java-package ()
  "Get the Java package name of the class defined in the current buffer"
  (let ((firsttag (semantic-find-tag-by-overlay-next 0)))
    (if (and firsttag (semantic-tag-of-class-p firsttag 'package))
	(semantic-tag-name firsttag))))

(defun current-java-class ()
  "Get the fully qualified Java class name for the class defined at point"
  ;;nested classes should be handled fine
  (let* ((tlist (nreverse (semantic-find-tag-by-overlay))) cls)
    (while (and tlist (not (semantic-tag-of-class-p (car tlist) 'type)))
      (setq tlist (cdr tlist)))
    (if tlist (progn
		(setq cls (semantic-tag-name (car tlist))
		      tlist (cdr tlist))
		(while tlist
		  (setq cls (concat (semantic-tag-name (car tlist)) "$" cls)
			tlist (cdr tlist)))
		(concat (let ((pkg (current-java-package)))
			  (if pkg (concat (current-java-package) "."))) cls)))))

(defcustom gud-yt-command-name
  "java -agentlib:yt -cp . Test"
  "Default command to execute an executable under the YellowTree debugger."
  :type 'string
  :group 'gud) ; TODO allow $LIBYTPATH $CLASSPATH $MAINCLASS

;;   [0] Test;.main([Ljava/lang/String;)V - 0 (Test.java:11)
(defun gud-yt-marker-filter (string)
  (if (string-match "\\*? +\\[[0-9]+\\] \\(.*?\\)..*?(.*? - [0-9]+ (\\(.*?\\):\\(-?[0-9]+\\))" string)
      (let* ((classname (match-string 1 string))
	     (filename (match-string 2 string))
	     (linenum (string-to-number (match-string 3 string))))
	(if (and (>= linenum 0) (not (string-equal "<unknown>" filename)))
	    (let ((filepath (if (search "/" classname)
				(concat (replace-regexp-in-string
					 "^\\(.*\\/\\).*$" "\\1" classname)
					filename)
			      filename)))
	      (setq gud-last-frame (cons filepath linenum))))))
  string)

(defun gud-yt-break-at-line ()
  "Add a breakpoint at the current line"
  (let ((sig (current-internal-method-signature)))
    ;; comint-simple-send ?
    (if sig (process-send-string (get-buffer-process gud-comint-buffer)
				 (concat "stop in " sig ":"
					 (number-to-string (line-number-at-pos))
					 "\n")))))

(defvar gud-yt-history nil "History for YellowTree")
;;(defun gud-yt-find-file (

(defun yt (command-line)
  (interactive (list (gud-query-cmdline 'yt)))
  ;; gud-common-init sucks because it creates the comint buffer name as
  ;; *gud-x* where x is the first argument to the command line not starting
  ;; with '-'. I can copy it and change it, but not worth the trouble at
  ;; this point.
  (gud-common-init command-line nil 'gud-yt-marker-filter)
  (set (make-local-variable 'gud-minor-mode) 'yt)
  ;; TODO commands - gud-def
  (gud-def gud-step "step()" "\C-s" "Step one source line")
  (gud-def gud-next "next()" "\C-n" "Step one source line (skip functions)")
  ;; This conflicts with a comint thing.. C-c C-r and the other one sucks because of screen C-x C-a C-r
  ;;  (gud-def gud-cont "g()" "\C-r" "Continue program")
  (gud-def gud-up "up()" "<" "Up N stack frames")
  (gud-def gud-down "down()" ">" "Down N stack frames")
  (gud-def gud-go "g()" nil "Resume execution")
  (setq comint-prompt-regexp "^yt> ")
  (setq paragraph-start comint-prompt-regexp)
  (setq gud-filter-pending-text nil))

(provide 'yt)
