program JT65code

! Provides examples of message packing, bit and symbol ordering,
! Reed Solomon encoding, and other necessary details of the JT65
! protocol.

  use packjt
  character*22 msg,msgchk,msg0,msg1,decoded,cok*3,bad*1,msgtype*10,expected
  integer dgen(12),sent(63),tmp(63),recd(12),era(51)
  include 'testmsg.f90'

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage: jt65code "message"'
     print*,'       jt65code -t'
     go to 999
  endif

  call getarg(1,msg)                     !Get message from command line
  msgchk=msg
  call fmtmsg(msgchk,iz)
  nmsg=1
  if(msg(1:2).eq."-t") then
     if (NTEST+5 > MAXTEST) then
        write(*,*) "NTEST exceed MAXTEST"
     endif
     testmsg(NTEST+1)="KA1ABC WB9XYZ EN34 OOO"
     testmsg(NTEST+2)="KA1ABC WB9XYZ OOO"
     testmsg(NTEST+3)="RO"
     testmsg(NTEST+4)="RRR"
     testmsg(NTEST+5)="73"
     testmsgchk(NTEST+1)="KA1ABC WB9XYZ EN34 OOO"
     testmsgchk(NTEST+2)="KA1ABC WB9XYZ OOO"
     testmsgchk(NTEST+3)="RO"
     testmsgchk(NTEST+4)="RRR"
     testmsgchk(NTEST+5)="73"
     nmsg=NTEST+5
  endif

  write(*,1010)
1010 format("    Message                Decoded              Err? Type          Expected"/   &
            76("-"))

  do imsg=1,nmsg
     if(nmsg.gt.1) then 
        msg=testmsg(imsg)
        msgchk=testmsgchk(imsg)
     endif

     call fmtmsg(msg,iz)                    !To upper, collapse mult blanks
     msg0=msg                               !Input message
     call chkmsg(msg,cok,nspecial,flip)     !See if it includes "OOO" report
     msg1=msg                               !Message without "OOO"

     if(nspecial.gt.0) then                  !or is a shorthand message
        if(nspecial.eq.2) decoded="RO"
        if(nspecial.eq.3) decoded="RRR"
        if(nspecial.eq.4) decoded="73"
        itype=-1
        msgtype="Shorthand"
        go to 10
     endif

     call packmsg(msg1,dgen,itype) !Pack message into 12 six-bit bytes
     msgtype=""
     if(itype.eq.1) msgtype="Std Msg"
     if(itype.eq.2) msgtype="Type 1 pfx"
     if(itype.eq.3) msgtype="Type 1 sfx"
     if(itype.eq.4) msgtype="Type 2 pfx"
     if(itype.eq.5) msgtype="Type 2 sfx"
     if(itype.eq.6) msgtype="Free text"

     call rs_encode(dgen,sent)               !RS encode
     call interleave63(sent,1)               !Interleave channel symbols
     call graycode(sent,63,1,sent)           !Apply Gray code

     call graycode(sent,63,-1,tmp)           !Remove Gray code
     call interleave63(tmp,-1)               !Remove interleaving
     call rs_decode(tmp,era,0,recd,nerr)     !Decode the message
     call unpackmsg(recd,decoded)            !Unpack the user message
     if(cok.eq."OOO") decoded(20:22)=cok
     call fmtmsg(decoded,iz)

10   bad=" "
     expected = 'EXACT'
     if(decoded.ne.msg0) then
        bad="*"
        if(decoded(1:13).eq.msg0(1:13) .and.                             &
             decoded(14:22).eq. '         ') expected = 'TRUNCATED'
     endif
     write(*,1020) imsg,msg0,decoded,bad,itype,msgtype,expected
1020 format(i2,'.',1x,a22,1x,a22,1x,a1,i3,":",a10,2x,a22)
  enddo

  if(nmsg.eq.1 .and. nspecial.eq.0) then
     write(*,1030) dgen
1030 format(/'Packed message, 6-bit symbols ',12i3) !Display packed symbols

     write(*,1040) sent
1040 format(/'Information-carrying channel symbols'/(i5,20i3))
  endif

999 end program JT65code
