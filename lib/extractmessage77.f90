subroutine extractmessage77(decoded77,msgreceived)
  use packjt

  character*22 msgreceived
  character*77 cbits
  integer*1 decoded77(77)
  integer*4 i4Dec6BitWords(12)

  write(cbits,'(77i1)') decoded77
!**** Temporary: For now, just handle i5bit=0.
  read(cbits,'(12b6)') i4Dec6BitWords 
  read(cbits,'(72x,i5.5)') i5bit
  if( i5bit .eq. 0 ) then
    call unpackmsg(i4Dec6BitWords,msgreceived)
  endif
  return
end subroutine extractmessage77
