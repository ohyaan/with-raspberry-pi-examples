set confirm off
break *width_demo
run
stepi
if $x0 != 0xffffffffffffffff
    quit 1
end
stepi
if $x0 != 0x1234
    quit 1
end
kill
quit 0
