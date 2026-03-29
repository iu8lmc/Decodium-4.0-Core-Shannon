module legacy_jt_async_mod
  use jt4_decode
  use jt65_decode
  use jt9_decode
  implicit none
  include 'constants.f90'

  integer, parameter :: MAX_ASYNC_LINES = 200

  type :: async_result_collector
     integer :: nasync_lines = 0
     integer :: nutc = 0
     integer :: minsync = 0
     logical :: bvhf = .false.
     character(len=80) :: results(MAX_ASYNC_LINES)
  end type

  type, extends(jt4_decoder) :: async_jt4_dec
     type(async_result_collector) :: collector
     real, allocatable :: cached_dd(:)
  end type

  type, extends(jt65_decoder) :: async_jt65_dec
     type(async_result_collector) :: collector
     real, allocatable :: cached_dd(:)
  end type

  type, extends(jt9_decoder) :: async_jt9_dec
     type(async_result_collector) :: collector
  end type

contains

  subroutine reset_async_results(collector, nutc, minsync, bvhf)
    type(async_result_collector), intent(inout) :: collector
    integer, intent(in) :: nutc
    integer, intent(in) :: minsync
    logical, intent(in) :: bvhf

    collector%nasync_lines = 0
    collector%nutc = nutc
    collector%minsync = minsync
    collector%bvhf = bvhf
    collector%results = ' '
  end subroutine reset_async_results

  subroutine append_async_line(collector, line)
    type(async_result_collector), intent(inout) :: collector
    character(len=*), intent(in) :: line
    if(collector%nasync_lines .lt. MAX_ASYNC_LINES) then
       collector%nasync_lines = collector%nasync_lines + 1
       collector%results(collector%nasync_lines) = line
    endif
  end subroutine append_async_line

  subroutine async_jt4_callback(this,snr,dt,freq,have_sync,sync,is_deep,decoded0,qual,ich,is_average,ave)
    class(jt4_decoder), intent(inout) :: this
    integer, intent(in) :: snr
    real, intent(in) :: dt
    integer, intent(in) :: freq
    logical, intent(in) :: have_sync
    logical, intent(in) :: is_deep
    character(len=1), intent(in) :: sync
    character(len=22), intent(in) :: decoded0
    real, intent(in) :: qual
    integer, intent(in) :: ich
    logical, intent(in) :: is_average
    integer, intent(in) :: ave

    character(len=22) :: decoded
    character(len=3) :: cflags
    character(len=80) :: line

    if(ich.eq.-99) stop
    if (have_sync) then
       decoded=decoded0
       cflags='   '
       if(decoded.ne.'                      ') then
          cflags='f  '
          if(is_deep) then
             cflags='d  '
             write(cflags(2:2),'(i1)') min(int(qual),9)
             if(qual.ge.10.0) cflags(2:2)='*'
             if(qual.lt.3.0) decoded(22:22)='?'
          endif
          if(is_average) then
             write(cflags(3:3),'(i1)') min(ave,9)
             if(ave.ge.10) cflags(3:3)='*'
             if(cflags(1:1).eq.'f') cflags=cflags(1:1)//cflags(3:3)//' '
          endif
       endif
       select type(this)
       type is (async_jt4_dec)
          write(line,1000) this%collector%nutc,snr,dt,freq,sync,decoded,cflags
       class default
          return
       end select
1000   format(i4.4,i4,f5.1,i5,1x,'$',a1,1x,a22,1x,a3)
    else
       select type(this)
       type is (async_jt4_dec)
          write(line,1001) this%collector%nutc,snr,dt,freq
       class default
          return
       end select
1001   format(i4.4,i4,f5.1,i5)
    end if

    select type(this)
    type is (async_jt4_dec)
       call append_async_line(this%collector, line)
    end select
  end subroutine async_jt4_callback

  subroutine async_jt4_average(this, used, utc, sync, dt, freq, flip)
    class(jt4_decoder), intent(inout) :: this
    logical, intent(in) :: used
    integer, intent(in) :: utc
    real, intent(in) :: sync
    real, intent(in) :: dt
    integer, intent(in) :: freq
    logical, intent(in) :: flip
    character(len=1) :: cused, csync

    cused = '.'
    csync = '*'
    if (used) cused = '$'
    if (flip) csync = '$'
    write(14,1000) cused,utc,sync,dt,freq,csync
1000 format(a1,i5.4,f6.1,f6.2,i6,1x,a1)
  end subroutine async_jt4_average

  subroutine async_jt65_callback(this,sync,snr,dt,freq,drift,nflip,width,decoded0,ft,qual,nsmo,nsum,minsync)
    class(jt65_decoder), intent(inout) :: this
    real, intent(in) :: sync
    integer, intent(in) :: snr
    real, intent(in) :: dt
    integer, intent(in) :: freq
    integer, intent(in) :: drift
    integer, intent(in) :: nflip
    real, intent(in) :: width
    character(len=22), intent(in) :: decoded0
    integer, intent(in) :: ft
    integer, intent(in) :: qual
    integer, intent(in) :: nsmo
    integer, intent(in) :: nsum
    integer, intent(in) :: minsync

    integer :: i, nap, n
    logical :: is_deep, is_average
    character(len=22) :: decoded
    character(len=2) :: csync
    character(len=3) :: cflags
    character(len=80) :: line

    decoded = decoded0
    cflags = '   '
    is_deep = ft.eq.2

    if(ft.eq.0 .and. minsync.ge.0 .and. int(sync).lt.minsync) then
       select type(this)
       type is (async_jt65_dec)
          write(line,1011) this%collector%nutc,snr,dt,freq
       class default
          return
       end select
1011   format(i4.4,i4,f5.1,i5)
       select type(this)
       type is (async_jt65_dec)
          call append_async_line(this%collector, line)
       end select
       return
    endif

    is_average=nsum.ge.2
    select type(this)
    type is (async_jt65_dec)
       if(this%collector%bvhf .and. ft.gt.0) then
       cflags='f  '
       if(is_deep) then
          cflags='d  '
          write(cflags(2:2),'(i1)') min(qual,9)
          if(qual.ge.10) cflags(2:2)='*'
          if(qual.lt.3) decoded(22:22)='?'
       endif
       if(is_average) then
          write(cflags(3:3),'(i1)') min(nsum,9)
          if(nsum.ge.10) cflags(3:3)='*'
       endif
       nap=ishft(ft,-2)
       if(nap.ne.0) then
          if(nsum.lt.2) write(cflags(1:3),'(a1,i1," ")') 'a',nap
          if(nsum.ge.2) write(cflags(1:3),'(a1,2i1)') 'a',nap,min(nsum,9)
       endif
       endif
    class default
       return
    end select

    csync='# '
    i=0
    select type(this)
    type is (async_jt65_dec)
       if(this%collector%bvhf .and. nflip.ne.0 .and. sync.ge.max(0.0,float(this%collector%minsync))) then
       csync='#*'
       if(nflip.eq.-1) then
          csync='##'
          if(decoded.ne.'                      ') then
             do i=22,1,-1
                if(decoded(i:i).ne.' ') exit
             enddo
             if(i.gt.18) i=18
             decoded(i+2:i+4)='OOO'
          endif
       endif
       endif
    class default
       return
    end select
    n=len(trim(decoded))
    if(n.eq.2 .or. n.eq.3) csync='# '
    if(cflags(1:1).eq.'f') then
       cflags(2:2)=cflags(3:3)
       cflags(3:3)=' '
    endif
    select type(this)
    type is (async_jt65_dec)
       write(line,1010) this%collector%nutc,snr,dt,freq,csync,decoded,cflags
    class default
       return
    end select
1010 format(i4.4,i4,f5.1,i5,1x,a2,1x,a22,1x,a3)

    select type(this)
    type is (async_jt65_dec)
       call append_async_line(this%collector, line)
    end select
  end subroutine async_jt65_callback

  subroutine async_jt9_callback(this, sync, snr, dt, freq, drift, decoded)
    class(jt9_decoder), intent(inout) :: this
    real, intent(in) :: sync
    integer, intent(in) :: snr
    real, intent(in) :: dt
    real, intent(in) :: freq
    integer, intent(in) :: drift
    character(len=22), intent(in) :: decoded

    character(len=80) :: line

    if(sync.lt.-999.0 .or. drift.eq.-99999) stop
    select type(this)
    type is (async_jt9_dec)
       write(line,1000) this%collector%nutc,snr,dt,nint(freq),decoded
    class default
       return
    end select
1000 format(i4.4,i4,f5.1,i5,1x,'@ ',1x,a22)

    select type(this)
    type is (async_jt9_dec)
       call append_async_line(this%collector, line)
    end select
  end subroutine async_jt9_callback

end module legacy_jt_async_mod


subroutine legacy_jt_async_decode(nmode, ss, id2, npts8, nzhsym, nutc, nfqso, ntol, ndepth, &
     nfa, nfb, nfsplit, nsubmode, nclearave, minsync, minw, emedelay, dttol, newdat, nagain, &
     n2pass, nrobust, ntrials, naggressive, nexp_decode, nqsoprogress, ljt65apon, mycall, &
     hiscall, hisgrid, temp_dir, outlines, nout)

  use legacy_jt_async_mod

  integer nmode, npts8, nzhsym, nutc, nfqso, ntol, ndepth, nfa, nfb, nfsplit
  integer nsubmode, nclearave, minsync, minw, newdat, nagain, n2pass, nrobust
  integer ntrials, naggressive, nexp_decode, nqsoprogress, ljt65apon, nout
  real ss(184,NSMAX)
  integer*2 id2(NTMAX*12000)
  real emedelay, dttol
  character*12 mycall, hiscall
  character*6 hisgrid
  character*(*) temp_dir
  character*80 outlines(MAX_ASYNC_LINES)

  type(async_jt4_dec), allocatable :: jt4dec
  type(async_jt65_dec), allocatable :: jt65dec
  type(async_jt9_dec), allocatable :: jt9dec
  integer i
  integer jz
  integer listutc_dummy(10)
  logical lnewdat
  logical lnagain
  logical lnclearave
  logical lnrobust
  logical ljt65ap

  outlines = ' '
  listutc_dummy = 0
  allocate(jt4dec, jt65dec, jt9dec)
  lnewdat = newdat .ne. 0
  lnagain = nagain .ne. 0
  lnclearave = nclearave .ne. 0
  lnrobust = nrobust .ne. 0
  ljt65ap = ljt65apon .ne. 0

  call reset_async_results(jt4dec%collector, nutc, minsync, iand(nexp_decode,64).ne.0)
  call reset_async_results(jt65dec%collector, nutc, minsync, iand(nexp_decode,64).ne.0)
  call reset_async_results(jt9dec%collector, nutc, minsync, iand(nexp_decode,64).ne.0)

  if(nmode.eq.4 .or. nmode.eq.65) then
     open(14,file=trim(temp_dir)//'/avemsg.txt',status='unknown')
  endif

  if(nmode.eq.4) then
     if(.not. allocated(jt4dec%cached_dd)) allocate(jt4dec%cached_dd(52*12000))
     jz=52*12000
     if(lnewdat) then
        call wav11(id2,jz,jt4dec%cached_dd)
     else
        jz=52*11025
     endif
     call jt4dec%decode(async_jt4_callback,jt4dec%cached_dd,jz,nutc,nfqso,ntol,emedelay,dttol, &
          lnagain,ndepth,lnclearave,minsync,minw,nsubmode,mycall,hiscall,hisgrid, &
          0,listutc_dummy,async_jt4_average)

  else if(nmode.eq.65) then
     if(.not. allocated(jt65dec%cached_dd)) allocate(jt65dec%cached_dd(60*12000))
     if(lnewdat) then
        jt65dec%cached_dd = 0.0
        jt65dec%cached_dd(1:52*12000)=id2(1:52*12000)
     endif
     call jt65dec%decode(async_jt65_callback,jt65dec%cached_dd,52*12000,lnewdat,nutc,nfa,nfb,nfqso, &
          ntol,nsubmode,minsync,lnagain,n2pass,lnrobust,ntrials,naggressive,ndepth, &
          emedelay,lnclearave,mycall,hiscall,hisgrid,nexp_decode,nqsoprogress,ljt65ap)

  else if(nmode.eq.9) then
     call jt9dec%decode(async_jt9_callback,ss,id2,nfqso,lnewdat,npts8,nfa,nfsplit,nfb, &
          ntol,nzhsym,lnagain,ndepth,nmode,nsubmode,nexp_decode)
  endif

  if(nmode.eq.4 .or. nmode.eq.65) then
     close(14)
  endif

  nout = 0
  if(nmode.eq.4) then
     nout = jt4dec%collector%nasync_lines
     do i = 1, nout
        outlines(i) = jt4dec%collector%results(i)
     enddo
  else if(nmode.eq.65) then
     nout = jt65dec%collector%nasync_lines
     do i = 1, nout
        outlines(i) = jt65dec%collector%results(i)
     enddo
  else if(nmode.eq.9) then
     nout = jt9dec%collector%nasync_lines
     do i = 1, nout
        outlines(i) = jt9dec%collector%results(i)
     enddo
  endif

  if(allocated(jt4dec%cached_dd)) deallocate(jt4dec%cached_dd)
  if(allocated(jt65dec%cached_dd)) deallocate(jt65dec%cached_dd)
  deallocate(jt4dec, jt65dec, jt9dec)

  return
end subroutine legacy_jt_async_decode
