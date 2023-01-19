#!/bin/bash
echo "Content-Type: text/html"
echo
echo "<HTML><BODY>"
echo "<CENTER>Today is:</CENTER>"
echo "<CENTER><B>"
date
echo "</B>"
echo "</CENTER>"
echo "<center>"
echo "<P style=color:$color>-$color-This is my color</P>"
echo "</center>"
echo "</BODY></HTML>"