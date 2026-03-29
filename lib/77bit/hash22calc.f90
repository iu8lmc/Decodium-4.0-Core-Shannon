program hash22calc 
! Given a valid callsign, calculate and print its 22-bit hash.

  use ftx_pack77_c_api, only: ftx_pack77_hash_call
  
  character*13 callsign
  character*1  c
  character*6  basecall
  logical      cok

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Given a valid callsign, print its 22-bit hash.'
     print*,'Usage: hash22calc <callsign>'
     print*,'  e.g. hash22calc W9ABC'
     go to 999
  endif
  call getarg(1,callsign)

! convert to upper case
  ilen=len(trim(callsign)) 
  do i=1, ilen
     c=callsign(i:i)
     if(c.ge.'a' .and. c.le.'z') c=char(ichar(c)-32)  !Force upper case
     callsign(i:i)=c
  enddo 

! check for a valid callsign
  call chkcall(callsign,basecall,cok)
  if(.not.cok) then
     print*,'Invalid callsign'
     print*,'Usage: hash22calc <callsign>'
     goto 999
  endif

! calculate the hash
  n22 = ftx_pack77_hash_call(callsign,22)
  write(*,'(a,i7.7)') callsign,n22

999 end program hash22calc

include '../chkcall.f90'
