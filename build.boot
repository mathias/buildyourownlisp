(set-env!
  :dependencies '[[boot/core                 "2.0.0-pre27" :scope "provided"]
                  [tailrecursion/boot-useful "0.1.3"       :scope "test"]])

(require '[tailrecursion.boot-useful :refer :all]
         '[boot.core :refer :all]
         '[boot.task-helpers :as helpers])

(def +version+ "0.0.1")

(useful! +version+)

(task-options!
  pom  [:project     'buildyourownlisp
        :version     +version+
        :description "Watch and compile sources for build your own lisp language"
        :url         "https://github.com/mathias/buildyourownlisp"
        :scm         {:url "https://github.com/mathias/buildyourownlisp"}
        :license     {:name "Eclipse Public License"
                      :url  "http://www.eclipse.org/legal/epl-v10.html"}])

(deftask watch-compile
  "Compile the src/ dir to an executable"
  []
  (comp
   (watch)
   (with-pre-wrap
     (apply helpers/dosh ["cc" "-std=c99" "-Wall" "src/lispy.c" "src/mpc.c" "-ledit" "-lm" "-o" "lispy"]))))
