mysh is a very simple shell with I/O redirection feature.

General syntax for executing any program/command -
mysh> Command arg1 arg2 ... argN
[Output of Command shown here]
mysh>


It supports the following I/O redirections -

    1) Redirecting input of a command from a file.
        example: wc < a.txt
    2) Redirecting output of a command to a file.
        example: ls > out.txt
    3) Piping, which redirects output of one command to the input of the other
        example: ls | wc

It also supports a combination of all the above redirects.
examples: ls | wc > out.txt
          cat < out.txt | wc
          cat < out.txt | wc | wc > out1.txt
          ls | grep mysh > mysh_.txt




