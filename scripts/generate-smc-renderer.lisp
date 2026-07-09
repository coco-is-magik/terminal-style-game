;;;; scripts/generate-smc-renderer.lisp — project-specific SMC warm-cache + C generator
;;;;
;;;; Usage:
;;;;   sbcl --script scripts/generate-smc-renderer.lisp <output-file>
;;;;
;;;; This script loads the vendored SMC system, registers the extra
;;;; trigonometric operators the renderer needs (atan, tan), warms the
;;;; cache with renderer-specific expressions, then emits a generated C
;;;; dispatch table.  It does not modify upstream SMC source files.

(require :asdf)

;; Add the vendored SMC system to ASDF's search path.
(pushnew (merge-pathnames "vendor/src/smc/") asdf:*central-registry*)

(asdf:load-system :self-modifying-calculator)

;; Load the upstream generator.  It does not declare a package, so its
;; helper functions (warm-cache, generate-c-source) land in the
;; COMMON-LISP-USER package.
(load (merge-pathnames "vendor/src/smc/scripts/generate-c-source.lisp"))

(in-package :common-lisp-user)

;; Register renderer-specific operators that the upstream SMC does not yet
;; provide.  These are emitted as plain C math.h calls by the generator.
(smc:register-operator :atan (lambda (x) (atan x)))
(smc:register-operator :tan  (lambda (x) (tan x)))

;; Warm the cache with scalar and argumentized renderer expressions.
;; Variables are named to match the C argument order we will pass to
;; smc_call_double in the adapter layer.
;;
;; We explicitly evaluate each expression first so that any parse/eval
;; errors are printed, then call warm-cache to populate the global cache
;; for the generator.
;; Warm the cache with renderer-specific expressions.
;; These expressions match the smc_render_opt.c wrappers.
;; Variables are named to match the C argument order.
(defun warm-cache-renderer ()
  "Evaluate and cache renderer-specific expressions."
  (dolist (entry '("2+3*4"
                   "(2+3)*4"
                   "10-4/2"
                   "1.5*2"

                   ;; Ray angle per column: atan(camera_x * tan(fov / 2.0))
                   ;; args: camera_x, fov
                   ("atan(x * tan(y / 2.0))" . ((:x . 0.5) (:y . 1.5707963267948966)))

                   ;; Fisheye correction: dist * cos(ray_angle - cam_angle)
                   ;; args: dist, ray_angle, cam_angle
                   ("x * cos(y - z)" . ((:x . 5.0) (:y . 0.7853981633974483) (:z . 0.7853981633974483)))

                   ;; Ceiling/floor true distance: currentDist / cos(ray_angle - cam_angle)
                   ;; args: currentDist, ray_angle, cam_angle
                   ("x / cos(y - z)" . ((:x . 10.0) (:y . 0.7853981633974483) (:z . 0.7853981633974483)))

                   ;; Light billboard screen X: tan(angle_diff) / tan(fov / 2.0)
                   ;; args: angle_diff, fov
                   ("tan(x) / tan(y / 2.0)" . ((:x . 0.1) (:y . 1.5707963267948966))))
    (let ((expr (if (consp entry) (car entry) entry))
          (vars (if (consp entry) (cdr entry) nil)))
      (format t "Warming cache for: ~A~%" expr)
      (if vars
          (smc:evaluate (smc:parse expr) :variables vars)
          (smc:run-calculator expr)))))

;; Generate the C dispatch table.  The output path is taken from the first
;; command-line argument when invoked as a script; otherwise default to
;; build/smc_generated.c.
(let ((path (if (and sb-ext:*posix-argv* (> (length sb-ext:*posix-argv*) 1))
                (pathname (second sb-ext:*posix-argv*))
                (merge-pathnames "build/smc_generated.c"))))
  (warm-cache-renderer)
  (generate-c-source path)

  ;; Verify that the renderer expressions actually made it into the table.
  ;; The upstream warm-cache helper swallows errors silently, so this is
  ;; the only way to be sure the generated table is usable.
  (let ((required-exprs '("atan((args[0] * tan((args[1] / 2.0))))"
                          "(args[0] * cos((args[1] - args[2])))"
                          "(args[0] / cos((args[1] - args[2])))"
                          "(tan(args[0]) / tan((args[1] / 2.0)))"))
        (content (with-open-file (s path)
                   (let ((buf (make-string (file-length s))))
                     (read-sequence buf s)
                     buf))))
    (dolist (expr required-exprs)
      (unless (search expr content)
        (format *error-output* "ERROR: generated table missing expression: ~A~%" expr)
        (sb-ext:exit :code 1))))

  (format t "Generated renderer SMC dispatch table: ~A (~D expressions)~%"
          path (smc:cache-size smc:*global-cache*)))

(sb-ext:exit)
