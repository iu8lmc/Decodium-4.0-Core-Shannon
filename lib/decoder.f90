subroutine multimode_decoder(ss,id2,params,nfsample)

!$ use omp_lib
  use prog_args
  use iso_c_binding, only: c_char, c_short, c_int, c_null_char
  use timer_module, only: timer
  use jt4_decode
  use jt65_decode
  use jt9_decode
  use fst4_decode

  include 'jt9com.f90'
  include 'timer_common.inc'

  type, extends(jt4_decoder) :: counting_jt4_decoder
     integer :: decoded
  end type counting_jt4_decoder

  type, extends(jt65_decoder) :: counting_jt65_decoder
     integer :: decoded
  end type counting_jt65_decoder

  type, extends(jt9_decoder) :: counting_jt9_decoder
     integer :: decoded
  end type counting_jt9_decoder

  type, extends(fst4_decoder) :: counting_fst4_decoder
     integer :: decoded
  end type counting_fst4_decoder

  integer ndelay
  type(params_block) :: params
  data ndelay/0/
  real ss(184,NSMAX)
  logical baddata,newdat65,newdat9,single_decode,bVHF,bad0,ex,supported_mode
  logical native_decode_mode
  logical lprinthash22
  integer*2 id2(NTMAX*12000)
  real*4, allocatable :: dd(:)
  character(len=20) :: datetime
  character(len=6) :: mygrid, hisgrid
  character(len=12) :: mycall, hiscall, mybcall, hisbcall
  character*60 line
  integer :: ios_dd
  data ntr0/-1/
  save
  type(counting_jt4_decoder) :: my_jt4
  type(counting_jt65_decoder) :: my_jt65
  type(counting_jt9_decoder) :: my_jt9
  type(counting_fst4_decoder) :: my_fst4
  logical :: decoded_file_opened
  character(kind=c_char) :: temp_dir_c(500)
  integer(c_int) :: native_decoded_count

  interface
     subroutine ftx_native_decode_and_emit_params_c(iwave, params, temp_dir, decoded_count) &
          bind(C, name='ftx_native_decode_and_emit_params_c')
       use iso_c_binding, only: c_char, c_short, c_int
       import :: params_block
       integer(c_short) :: iwave(*)
       type(params_block), intent(in) :: params
       character(kind=c_char) :: temp_dir(*)
       integer(c_int) :: decoded_count
     end subroutine ftx_native_decode_and_emit_params_c
  end interface

  if(.not.params%newdat .and. params%ntr.gt.ntr0) go to 800
  ntr0=params%ntr
  rms=sqrt(dot_product(float(id2(1:180000)),float(id2(1:180000)))/180000.0)
  if(rms.lt.0.1) go to 800

! Cast C character arrays to Fortran character strings
  datetime=transfer(params%datetime, datetime)
  mycall=transfer(params%mycall,mycall)
  mybcall=transfer(params%mybcall,mybcall)
  hiscall=transfer(params%hiscall,hiscall)
  hisbcall=transfer(params%hisbcall,hisbcall)
  mygrid=transfer(params%mygrid,mygrid)
  hisgrid=transfer(params%hisgrid,hisgrid)
! Initialize decode counts
  my_jt4%decoded = 0
  my_jt65%decoded = 0
  my_jt9%decoded = 0
  my_fst4%decoded = 0
  native_decoded_count = 0_c_int

  native_decode_mode = params%nmode.eq.2 .or. params%nmode.eq.5 .or. params%nmode.eq.8 .or. &
       params%nmode.eq.66

  supported_mode = params%nmode.eq.2 .or. params%nmode.eq.4 .or. params%nmode.eq.5 .or. &
       params%nmode.eq.65 .or. params%nmode.eq.66 .or. params%nmode.eq.(65+9) .or.       &
       params%nmode.eq.8 .or. params%nmode.eq.9 .or. params%nmode.eq.240 .or. params%nmode.eq.241 .or.           &
       params%nmode.eq.242
  if(.not.supported_mode) go to 800

  if(.not.native_decode_mode) then
     allocate(dd(NTMAX*12000), stat=ios_dd)
     if(ios_dd.ne.0) go to 800
  endif

! For testing only: return Rx messages stored in a file as decodes
  inquire(file='rx_messages.txt',exist=ex)
  if(ex) then
     if(params%nzhsym.eq.41) then
        open(39,file='rx_messages.txt',status='old')
        do i=1,9999
           read(39,'(a60)',end=5) line
           if(line(1:1).eq.' ' .or. line(1:1).eq.'-') go to 800
           write(*,'(a)') trim(line)
        enddo
5       close(39)
     endif
     go to 800
  endif

  ncontest=iand(params%nexp_decode,7)
  single_decode=iand(params%nexp_decode,32).ne.0
  bVHF=iand(params%nexp_decode,64).ne.0
  if(mod(params%nranera,2).eq.0) ntrials=10**(params%nranera/2)
  if(mod(params%nranera,2).eq.1) ntrials=3*10**(params%nranera/2)
  if(params%nranera.eq.0) ntrials=0

  nfail=0
  ios13=0
  decoded_file_opened=.false.
10 if (params%nagain) then
     if(native_decode_mode) then
        ios13=0
     else
        open(13,file=trim(temp_dir)//'/decoded.txt',status='unknown',         &
             position='append',iostat=ios13)
     endif
  else
     if(native_decode_mode) then
        ios13=0
     else
        open(13,file=trim(temp_dir)//'/decoded.txt',status='unknown',iostat=ios13)
     endif
  endif
  if(ios13.ne.0) then
     nfail=nfail+1
     if(nfail.le.3) then
        call sleep_msec(10)
        go to 10
     endif
  else if(.not.native_decode_mode) then
     decoded_file_opened=.true.
  endif

  if(native_decode_mode) then
     temp_dir_c=c_null_char
     do i=1,min(len_trim(temp_dir),size(temp_dir_c))
        temp_dir_c(i)=temp_dir(i:i)
     enddo
     call timer('decftx  ',0)
     native_decoded_count=0_c_int
     call ftx_native_decode_and_emit_params_c(id2,params,temp_dir_c,native_decoded_count)
     params%nclearave=.false.
     call timer('decftx  ',1)
     go to 800
  endif

  if(params%nmode.eq.240) then
     ! We're in FST4 mode
     ndepth=iand(params%ndepth,3)
     iwspr=0
     lprinthash22=.false.
     params%nsubmode=0
     call timer('dec_fst4',0)
     call my_fst4%decode(fst4_decoded,id2,params%nutc,                &
          params%nQSOProgress,params%nfa,params%nfb,                  &
          params%nfqso,ndepth,params%ntr,params%nexp_decode,          &
          params%ntol,params%emedelay,logical(params%nagain),         &
          logical(params%lapcqonly),mycall,hiscall,iwspr,lprinthash22)
     call timer('dec_fst4',1)
     go to 800
  endif

  if(params%nmode.eq.241 .or. params%nmode.eq.242) then
     ! We're in FST4W mode
     ndepth=iand(params%ndepth,3)
     iwspr=1
     lprinthash22=.false.
     if(params%nmode.eq.242) lprinthash22=.true. 
     call timer('dec_fst4',0)
     call my_fst4%decode(fst4_decoded,id2,params%nutc,                &
          params%nQSOProgress,params%nfa,params%nfb,                  &
          params%nfqso,ndepth,params%ntr,params%nexp_decode,          &
          params%ntol,params%emedelay,logical(params%nagain),         &
          logical(params%lapcqonly),mycall,hiscall,iwspr,lprinthash22)
     call timer('dec_fst4',1)
     go to 800
  endif

  ! Zap data at start that might come from T/R switching transient?
  nadd=100
  k=0
  bad0=.false.
  do i=1,240
     sq=0.
     do n=1,nadd
        k=k+1
        sq=sq + float(id2(k))**2
     enddo
     rms=sqrt(sq/nadd)
     if(rms.gt.10000.0) then
        bad0=.true.
        kbad=k
        rmsbad=rms
     endif
  enddo
  if(bad0) then
     nz=min(NTMAX*12000,kbad+100)
              !     id2(1:nz)=0                ! temporarily disabled as it can breaak the JT9 decoder, maybe others
  endif

  if(params%nmode.eq.4 .or. params%nmode.eq.65) open(14,file=trim(temp_dir)// &
       '/avemsg.txt',status='unknown')

  if(params%nmode.eq.4) then
     jz=52*nfsample
     if(params%newdat) then
        if(nfsample.eq.12000) call wav11(id2,jz,dd)
        if(nfsample.eq.11025) dd(1:jz)=id2(1:jz)
     else
        jz=52*11025
     endif
     call my_jt4%decode(jt4_decoded,dd,jz,params%nutc,params%nfqso,         &
          params%ntol,params%emedelay,params%dttol,logical(params%nagain),  &
          params%ndepth,logical(params%nclearave),params%minsync,           &
          params%minw,params%nsubmode,mycall,hiscall,         &
          hisgrid,params%nlist,params%listutc,jt4_average)
     go to 800
  endif

  npts65=52*12000
  if(baddata(id2,npts65)) then
     nsynced=0
     ndecoded=0
     go to 800
  endif
  
  ntol65=params%ntol              !### is this OK? ###
  newdat65=params%newdat
  newdat9=params%newdat

  call omp_set_dynamic(.true.)

!$omp parallel sections num_threads(2) shared(ndecoded) if(.true.) !iif() needed on Mac

!$omp section
  if(params%nmode.eq.65) then                       ! We're in JT65 mode     
     if(newdat65) dd(1:npts65)=id2(1:npts65)
     nf1=params%nfa
     nf2=params%nfb
     call timer('jt65a   ',0)
     call my_jt65%decode(jt65_decoded,dd,npts65,newdat65,params%nutc,      &
          nf1,nf2,params%nfqso,ntol65,params%nsubmode,params%minsync,      &
          logical(params%nagain),params%n2pass,logical(params%nrobust),    &
          ntrials,params%naggressive,params%ndepth,params%emedelay,        &
          logical(params%nclearave),mycall,hiscall,                        &
          hisgrid,params%nexp_decode,params%nQSOProgress,                  &
          logical(params%ljt65apon))
     call timer('jt65a   ',1)

  else if(params%nmode.eq.9 .or. (params%nmode.eq.(65+9) .and.             &
       params%ntxmode.eq.9)) then
              ! We're in JT9 mode, or should do JT9 first
     call timer('decjt9  ',0)
     call my_jt9%decode(jt9_decoded,ss,id2,params%nfqso,                   &
          newdat9,params%npts8,params%nfa,params%nfsplit,params%nfb,       &
          params%ntol,params%nzhsym,logical(params%nagain),params%ndepth,  &
          params%nmode,params%nsubmode,params%nexp_decode)
     call timer('decjt9  ',1)
  endif

!$omp section
  if(params%nmode.eq.(65+9)) then       !Do the other mode (we're in dual mode)
     if (params%ntxmode.eq.9) then
        if(newdat65) dd(1:npts65)=id2(1:npts65)
        nf1=params%nfa
        nf2=params%nfb
        call timer('jt65a   ',0)
        call my_jt65%decode(jt65_decoded,dd,npts65,newdat65,params%nutc,   &
             nf1,nf2,params%nfqso,ntol65,params%nsubmode,params%minsync,   &
             logical(params%nagain),params%n2pass,logical(params%nrobust), &
             ntrials,params%naggressive,params%ndepth,params%emedelay,     &
             logical(params%nclearave),mycall,hiscall,       &
             hisgrid,params%nexp_decode,params%nQSOProgress,        &
             logical(params%ljt65apon))
        call timer('jt65a   ',1)
     else
        call timer('decjt9  ',0)
        call my_jt9%decode(jt9_decoded,ss,id2,params%nfqso,                &
             newdat9,params%npts8,params%nfa,params%nfsplit,params%nfb,    &
             params%ntol,params%nzhsym,logical(params%nagain),             &
             params%ndepth,params%nmode,params%nsubmode,params%nexp_decode)
        call timer('decjt9  ',1)
     end if
  endif

!$omp end parallel sections


! JT65 is not yet producing info for nsynced, ndecoded.
800 ndecoded = my_jt4%decoded + my_jt65%decoded + my_jt9%decoded +       &
         int(native_decoded_count) + my_fst4%decoded
  if(.not.lquiet) write(*,1010) nsynced,ndecoded,navg0
1010 format('<DecodeFinished>',2i4,i9)
  call flush(6)
  if(allocated(dd)) deallocate(dd)
  if(decoded_file_opened) close(13)
  if(params%nmode.eq.4 .or. params%nmode.eq.65) close(14)
  return

contains

  subroutine jt4_decoded(this,snr,dt,freq,have_sync,sync,is_deep,    &
       decoded0,qual,ich,is_average,ave)
    implicit none
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

    character*22 decoded
    character*3 cflags

    if(ich.eq.-99) stop                         !Silence compiler warning
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
       write(*,1000) params%nutc,snr,dt,freq,sync,decoded,cflags
1000   format(i4.4,i4,f5.1,i5,1x,'$',a1,1x,a22,1x,a3)
    else
       write(*,1000) params%nutc,snr,dt,freq
    end if

    select type(this)
    type is (counting_jt4_decoder)
       this%decoded = this%decoded + 1
    end select
  end subroutine jt4_decoded

  subroutine jt4_average (this, used, utc, sync, dt, freq, flip)
    implicit none
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
  end subroutine jt4_average

  subroutine jt65_decoded(this,sync,snr,dt,freq,drift,nflip,width,     &
       decoded0,ft,qual,nsmo,nsum,minsync)

    use jt65_decode
    implicit none

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

    integer i,nap
    logical is_deep,is_average
    character decoded*22,csync*2,cflags*3

    !$omp critical(decode_results)
    decoded=decoded0
    cflags='   '
    is_deep=ft.eq.2

    if(ft.eq.0 .and. minsync.ge.0 .and. int(sync).lt.minsync) then
       write(*,1010) params%nutc,snr,dt,freq
    else
       is_average=nsum.ge.2
       if(bVHF .and. ft.gt.0) then
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
       csync='# '
       i=0
       if(bVHF .and. nflip.ne.0 .and.                         &
            sync.ge.max(0.0,float(minsync))) then
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
       n=len(trim(decoded))
       if(n.eq.2 .or. n.eq.3) csync='# '
       if(cflags(1:1).eq.'f') then
          cflags(2:2)=cflags(3:3)
          cflags(3:3)=' '
       endif
       write(*,1010) params%nutc,snr,dt,freq,csync,decoded,cflags
1010   format(i4.4,i4,f5.1,i5,1x,a2,1x,a22,1x,a3)
    endif
    if(ios13.eq.0) write(13,1012) params%nutc,nint(sync),snr,dt,    &
         float(freq),drift,decoded,ft,nsum,nsmo
1012 format(i4.4,i4,i5,f6.2,f8.0,i4,3x,a22,' JT65',3i3)
    call flush(6)

    !$omp end critical(decode_results)
    select type(this)
    type is (counting_jt65_decoder)
       this%decoded = this%decoded + 1
    end select
  end subroutine jt65_decoded
  
  subroutine jt9_decoded (this, sync, snr, dt, freq, drift, decoded)
    use jt9_decode
    implicit none

    class(jt9_decoder), intent(inout) :: this
    real, intent(in) :: sync
    integer, intent(in) :: snr
    real, intent(in) :: dt
    real, intent(in) :: freq
    integer, intent(in) :: drift
    character(len=22), intent(in) :: decoded

    !$omp critical(decode_results)

    write(*,1000) params%nutc,snr,dt,nint(freq),decoded
1000 format(i4.4,i4,f5.1,i5,1x,'@ ',1x,a22)
    if(ios13.eq.0) write(13,1002) params%nutc,nint(sync),snr,dt,freq,  &
         drift,decoded
1002 format(i4.4,i4,i5,f6.1,f8.0,i4,3x,a22,' JT9')
    call flush(6)
    !$omp end critical(decode_results)
    select type(this)
    type is (counting_jt9_decoder)
       this%decoded = this%decoded + 1
    end select
  end subroutine jt9_decoded

  subroutine fst4_decoded (this,nutc,sync,nsnr,dt,freq,decoded,nap,   &
       qual,ntrperiod,fmid,w50,bits77)

    use fst4_decode
    implicit none

    class(fst4_decoder), intent(inout) :: this
    integer, intent(in) :: nutc
    real, intent(in) :: sync
    integer, intent(in) :: nsnr
    real, intent(in) :: dt
    real, intent(in) :: freq
    character(len=37), intent(in) :: decoded
    integer, intent(in) :: nap
    real, intent(in) :: qual
    integer, intent(in) :: ntrperiod
    real, intent(in) :: fmid
    real, intent(in) :: w50
    integer*1, intent(in) :: bits77(77)

    character*2 annot
    character*37 decoded0
    character*70 line

    decoded0=decoded
    annot='  '
    if(nap.ne.0) then
       write(annot,'(a1,i1)') 'a',nap
       if(qual.lt.0.17) decoded0(37:37)='?'
    endif

    if(ntrperiod.lt.60) then
       write(line,1001) nutc,nsnr,dt,nint(freq),decoded0,annot
1001   format(i6.6,i4,f5.1,i5,' ` ',1x,a37,1x,a2)
       if(ios13.eq.0) write(13,1002) nutc,nint(sync),nsnr,dt,freq,0,decoded0
1002   format(i6.6,i4,i5,f6.1,f8.0,i4,3x,a37,' FST4')
    else
       write(line,1003) nutc,nsnr,dt,nint(freq),decoded0,annot
1003   format(i4.4,i4,f5.1,i5,' ` ',1x,a37,1x,a2,2f7.3)
       if(ios13.eq.0) write(13,1004) nutc,nint(sync),nsnr,dt,freq,0,decoded0
1004   format(i4.4,i4,i5,f6.1,f8.0,i4,3x,a37,' FST4')
    endif

    if(fmid.ne.-999.0) then
       if(w50.lt.0.95) write(line(65:70),'(f6.3)') w50
       if(w50.ge.0.95) write(line(65:70),'(f6.2)') w50
    endif

    write(*,1005) line
1005 format(a70)

    call flush(6)
    if(ios13.eq.0) call flush(13)

    select type(this)
    type is (counting_fst4_decoder)
       this%decoded = this%decoded + 1
    end select

    return
  end subroutine fst4_decoded

end subroutine multimode_decoder
