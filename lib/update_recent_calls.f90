subroutine update_recent_calls(call,calls_hrd,nsize)
character*12 call,calls_hrd(nsize)

 new=1
 do ic=1,nsize
   if( calls_hrd(ic).eq.call ) then
     new=0
   endif
 enddo

 if( new.eq.1 ) then
   do ic=nsize-1,1,-1
     calls_hrd(ic+1)(1:12)=calls_hrd(ic)(1:12)
   enddo
   calls_hrd(1)(1:12)=call(1:12)
 endif

 return
 end subroutine update_recent_calls
