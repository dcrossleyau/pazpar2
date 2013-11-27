\
  set term png
  set out "plot.png"
  #set yrange [0:300000]
  plot \
 "hp.data" using 0:1  with points  title "harry potter",  \
 "vw.data" using 0:1  with points  title "vietnam war",  \
 "wa.data" using 0:1  with points  title "water or fire or ice"

