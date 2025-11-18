$ip = "192.168.0.12"
$oidV = "1.3.6.1.4.1.19865.5.1.1.0"
$oidA = "1.3.6.1.4.1.19865.5.1.2.0"
$oidW = "1.3.6.1.4.1.19865.5.1.3.0"
$v = [double](snmpget -v1 -c public -Oqv $ip $oidV)
$a = [double](snmpget -v1 -c public -Oqv $ip $oidA)
$w = [double](snmpget -v1 -c public -Oqv $ip $oidW)
"{0:N2} V, {1:N3} A, {2:N1} W" -f $v, $a, $w