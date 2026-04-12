subroutine encode4(message,ncode)

  use packjt
  parameter (MAXCALLS=7000,MAXRPT=63)
  integer ncode(206)
  character*22 message          !Message to be generated
  character*3 cok               !'   ' or 'OOO'
  integer dgen(13)
  integer*1 data0(13),symbol(216)

  call chkmsg(message,cok,nspecial,flip)
  call packmsg(message,dgen,itype) !Pack 72-bit message into 12 six-bit symbols
  call entail(dgen,data0)
  call encode232(data0,206,symbol)       !Convolutional encoding
  call interleave4(symbol,1)             !Apply JT4 interleaving
  do i=1,206
     ncode(i)=symbol(i)
  enddo

end subroutine encode4
