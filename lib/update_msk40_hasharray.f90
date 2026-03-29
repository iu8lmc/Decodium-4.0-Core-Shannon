subroutine update_msk40_hasharray(recent_calls,nhasharray)

  use ftx_pack77_c_api, only: MAXRECENT
  character*37 hashmsg
  character*13 recent_calls(MAXRECENT)
  integer nhasharray(MAXRECENT,MAXRECENT)

  nhasharray=-1
  do i=1,MAXRECENT
    do j=i+1,MAXRECENT
      if( recent_calls(i)(1:1) .ne. ' ' .and. recent_calls(j)(1:1) .ne. ' ' ) then
        hashmsg=trim(recent_calls(i))//' '//trim(recent_calls(j))
        call fmtmsg(hashmsg,iz)
        call hash(hashmsg,37,ihash)
        ihash=iand(ihash,4095)
        nhasharray(i,j)=ihash
        hashmsg=trim(recent_calls(j))//' '//trim(recent_calls(i))
        call fmtmsg(hashmsg,iz)
        call hash(hashmsg,37,ihash)
        ihash=iand(ihash,4095)
        nhasharray(j,i)=ihash
      endif
    enddo
  enddo 

end subroutine update_msk40_hasharray
