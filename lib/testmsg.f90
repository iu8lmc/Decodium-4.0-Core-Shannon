  parameter (MAXTEST=75,NTEST=68)
  character*22 testmsg(MAXTEST)
  character*22 testmsgchk(MAXTEST)
  ! Test msgs should include the extremes for the different types
  ! See pfx.f90
  ! Type 1 P & A
  ! Type 1 1A & E5
  data testmsg(1:NTEST)/         &
       "CQ WB9XYZ EN34",         &
       "CQ DX WB9XYZ EN34",      &
       "QRZ WB9XYZ EN34",        &
       "KA1ABC WB9XYZ EN34",     &
       "KA1ABC WB9XYZ RO",       &
       "KA1ABC WB9XYZ -21",      &
       "KA1ABC WB9XYZ R-19",     &
       "KA1ABC WB9XYZ RRR",      &
       "KA1ABC WB9XYZ 73",       &
       "KA1ABC WB9XYZ",          &
       "CQ 010 WB9XYZ EN34",     &
       "CQ 999 WB9XYZ EN34",     &
       "CQ EU WB9XYZ EN34",      &
       "CQ WY WB9XYZ EN34",      &
       "1A/KA1ABC WB9XYZ",       &
       "E5/KA1ABC WB9XYZ",       &
       "KA1ABC 1A/WB9XYZ",       &
       "KA1ABC E5/WB9XYZ",       &
       "KA1ABC/P WB9XYZ",        &
       "KA1ABC/A WB9XYZ",        &
       "KA1ABC WB9XYZ/P",        &
       "KA1ABC WB9XYZ/A",        &
       "CQ KA1ABC/P",            &
       "CQ WB9XYZ/A",            &
       "QRZ KA1ABC/P",           &
       "QRZ WB9XYZ/A",           &
       "DE KA1ABC/P",            &
       "DE WB9XYZ/A",            &
       "CQ 1A/KA1ABC",           &
       "CQ E5/KA1ABC",           &
       "DE 1A/KA1ABC",           &
       "DE E5/KA1ABC",           &
       "QRZ 1A/KA1ABC",          &
       "QRZ E5/KA1ABC",          &
       "CQ WB9XYZ/1A",           &
       "CQ WB9XYZ/E5",           &
       "QRZ WB9XYZ/1A",          &
       "QRZ WB9XYZ/E5",          &
       "DE WB9XYZ/1A",           &
       "DE WB9XYZ/E5",           &
       "CQ A000/KA1ABC FM07",    &
       "CQ ZZZZ/KA1ABC FM07",    &
       "QRZ W4/KA1ABC FM07",     &
       "DE W4/KA1ABC FM07",      &
       "CQ W4/KA1ABC -22",       &
       "DE W4/KA1ABC -22",       &
       "QRZ W4/KA1ABC -22",      &
       "CQ W4/KA1ABC R-22",      &
       "DE W4/KA1ABC R-22",      &
       "QRZ W4/KA1ABC R-22",     &
       "DE W4/KA1ABC 73",        &
       "CQ KA1ABC FM07",         &
       "QRZ KA1ABC FM07",        &
       "DE KA1ABC/VE6 FM07",     &
       "CQ KA1ABC/VE6 -22",      &
       "DE KA1ABC/VE6 -22",      &
       "QRZ KA1ABC/VE6 -22",     &
       "CQ KA1ABC/VE6 R-22",     &
       "DE KA1ABC/VE6 R-22",     &
       "QRZ KA1ABC/VE6 R-22",    &
       "DE KA1ABC 73",           &
       "HELLO WORLD",            &
       "ZL4/KA1ABC 73",          &
       "KA1ABC XL/WB9XYZ",       &
       "KA1ABC WB9XYZ/W4",       &
       "DE KA1ABC/QRP 2W",       &
       "KA1ABC/1 WB9XYZ/1",      &
       "123456789ABCDEFGH"/
  data testmsgchk(1:NTEST)/      &
       "CQ WB9XYZ EN34",         &
       "CQ DX WB9XYZ EN34",      &
       "QRZ WB9XYZ EN34",        &
       "KA1ABC WB9XYZ EN34",     &
       "KA1ABC WB9XYZ RO",       &
       "KA1ABC WB9XYZ -21",      &
       "KA1ABC WB9XYZ R-19",     &
       "KA1ABC WB9XYZ RRR",      &
       "KA1ABC WB9XYZ 73",       &
       "KA1ABC WB9XYZ",          &
       "CQ 000 WB9XYZ EN34",     &
       "CQ 999 WB9XYZ EN34",     &
       "CQ EU WB9XYZ EN34",      &
       "CQ WY WB9XYZ EN34",      &
       "1A/KA1ABC WB9XYZ",       &
       "E5/KA1ABC WB9XYZ",       &
       "KA1ABC 1A/WB9XYZ",       &
       "KA1ABC E5/WB9XYZ",       &
       "KA1ABC/P WB9XYZ",        &
       "KA1ABC/A WB9XYZ",        &
       "KA1ABC WB9XYZ/P",        &
       "KA1ABC WB9XYZ/A",        &
       "CQ KA1ABC/P",            &
       "CQ WB9XYZ/A",            &
       "QRZ KA1ABC/P",           &
       "QRZ WB9XYZ/A",           &
       "DE KA1ABC/P",            &
       "DE WB9XYZ/A",            &
       "CQ 1A/KA1ABC",           &
       "CQ E5/KA1ABC",           &
       "DE 1A/KA1ABC",           &
       "DE E5/KA1ABC",           &
       "QRZ 1A/KA1ABC",          &
       "QRZ E5/KA1ABC",          &
       "CQ WB9XYZ/1A",           &
       "CQ WB9XYZ/E5",           &
       "QRZ WB9XYZ/1A",          &
       "QRZ WB9XYZ/E5",          &
       "DE WB9XYZ/1A",           &
       "DE WB9XYZ/E5",           &
       "CQ A000/KA1ABC FM07",    &
       "CQ ZZZZ/KA1ABC FM07",    &
       "QRZ W4/KA1ABC FM07",     &
       "DE W4/KA1ABC FM07",      &
       "CQ W4/KA1ABC -22",       &
       "DE W4/KA1ABC -22",       &
       "QRZ W4/KA1ABC -22",      &
       "CQ W4/KA1ABC R-22",      &
       "DE W4/KA1ABC R-22",      &
       "QRZ W4/KA1ABC R-22",     &
       "DE W4/KA1ABC 73",        &
       "CQ KA1ABC FM07",         &
       "QRZ KA1ABC FM07",        &
       "DE KA1ABC/VE6 FM07",     &
       "CQ KA1ABC/VE6 -22",      &
       "DE KA1ABC/VE6 -22",      &
       "QRZ KA1ABC/VE6 -22",     &
       "CQ KA1ABC/VE6 R-22",     &
       "DE KA1ABC/VE6 R-22",     &
       "QRZ KA1ABC/VE6 R-22",    &
       "DE KA1ABC 73",           &
       "HELLO WORLD",            &
       "ZL4/KA1ABC 73",          &
       "KA1ABC XL/WB9",          &
       "KA1ABC WB9XYZ",          &
       "DE KA1ABC/QRP",          &
       "KA1ABC/1 WB9X",          &
       "123456789ABCD"/
