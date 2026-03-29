module mskrtd_contexts
  use ftx_pack77_c_api, only: MAXRECENT
  implicit none
  integer, parameter :: MSKRTD_NZ=7168
  integer, parameter :: MSKRTD_NSPM=864
  integer, parameter :: MSKRTD_NFFT1=8192
  integer, parameter :: MSKRTD_NPATTERNS=4
  integer, parameter :: MSKRTD_NSHMEM=50

  type :: mskrtd_state
     logical :: initialized=.false.
     real :: tsec0=0.0
     integer :: nutc00=0
     real :: pnoise=-1.0
     complex :: cdat(MSKRTD_NFFT1)
     character(len=37) :: msglast='                                     '
     character(len=37) :: msglastswl='                                     '
     character(len=37) :: recent_shmsgs(MSKRTD_NSHMEM)
     character(len=13) :: recent_calls(MAXRECENT)
     integer :: nhasharray(MAXRECENT,MAXRECENT)=0
     integer :: nsnrlast=-99
     integer :: nsnrlastswl=-99
     character(len=13) :: last_mycall13=' '
     character(len=13) :: last_dxcall13=' '
  end type mskrtd_state

  type(mskrtd_state), target, save :: shared_mskrtd_state
end module mskrtd_contexts

subroutine mskrtd(id2,nutc0,tsec,ntol,nrxfreq,ndepth,mycall,hiscall,      &
     bshmsg,btrain,pcoeffs,bswl,datadir,line)

! Real-time decoder for MSK144.  
! Analysis block size = NZ = 7168 samples, t_block = 0.597333 s 
! Called from hspec() at half-block increments, about 0.3 s

  use mskrtd_contexts, only: mskrtd_state, shared_mskrtd_state,          &
       MSKRTD_NZ, MSKRTD_NSPM, MSKRTD_NFFT1, MSKRTD_NPATTERNS,           &
       MSKRTD_NSHMEM
  integer, parameter :: NZ=MSKRTD_NZ
  integer, parameter :: NSPM=MSKRTD_NSPM
  integer, parameter :: NFFT1=MSKRTD_NFFT1

  character*4 decsym                 !"&" for mskspd or "^" for long averages
  character*37 msgreceived           !Decoded message
  character*(*) line                  !Formatted line with UTC dB T Freq Msg
  character*(*) mycall,hiscall
  character*(*) datadir
  character*13 requested_mycall13,requested_dxcall13

  complex c(MSKRTD_NSPM)             !Coherently averaged complex data
  complex ct(MSKRTD_NSPM)

  integer*2 id2(MSKRTD_NZ)           !Raw 16-bit data
  integer iavmask(8)
  integer iavpatterns(8,MSKRTD_NPATTERNS)
  integer npkloc(10)

  real d(MSKRTD_NFFT1)
  real pow(8)
  real softbits(144)
  real xmc(MSKRTD_NPATTERNS)
  real*8 pcoeffs(5)

  logical*1 bshmsg,btrain,bswl
  logical*1 bshdecode
  logical*1 seenb4
  logical*1 bflag
  logical*1 bvar
  type(mskrtd_state), pointer :: state
 
  data iavpatterns/ &
       1,1,1,1,0,0,0,0, &
       0,0,1,1,1,1,0,0, &
       1,1,1,1,1,0,0,0, &
       1,1,1,1,1,1,1,0/
  data xmc/2.0,4.5,2.5,3.5/     !Used to set time at center of averaging mask
  state => shared_mskrtd_state

  requested_mycall13=' '
  requested_dxcall13=' '
  requested_mycall13(1:12)=mycall(1:12)
  requested_dxcall13(1:12)=hiscall(1:12)
  if((.not.state%initialized) .or.                                         &
       state%last_mycall13.ne.requested_mycall13 .or.                      &
       state%last_dxcall13.ne.requested_dxcall13) then
     state%tsec0=tsec
     state%nutc00=nutc0
     state%pnoise=-1.0
     state%recent_calls=' '
     do i=1,MSKRTD_NSHMEM
       state%recent_shmsgs(i)(1:37)=' '
     enddo
     state%msglast='                                     '
     state%msglastswl='                                     '
     state%nsnrlast=-99
     state%nsnrlastswl=-99
     state%nhasharray=0
     state%last_mycall13=requested_mycall13
     state%last_dxcall13=requested_dxcall13
     state%initialized=.true.
  endif
  fc=nrxfreq

! Dupe checking setup 
  if(state%nutc00.ne.nutc0 .or. tsec.lt.state%tsec0) then ! reset dupe checker
    state%msglast='                                     '
    state%msglastswl='                                     '
    state%nsnrlast=-99
    state%nsnrlastswl=-99
    state%nutc00=nutc0
  endif
  
  tframe=float(NSPM)/12000.0 
  line(1:1)=char(0)
  msgreceived='                                     '
  max_iterations=10
  niterations=0
  d(1:MSKRTD_NZ)=id2
  rms=sqrt(sum(d(1:MSKRTD_NZ)*d(1:MSKRTD_NZ))/MSKRTD_NZ)
  if(rms.lt.1.0) go to 999
  fac=1.0/rms
  d(1:MSKRTD_NZ)=fac*d(1:MSKRTD_NZ)
  d(MSKRTD_NZ+1:MSKRTD_NFFT1)=0.
  bvar=.true.
  if( btrain ) bvar=.false.   ! if training, turn off rx eq
  call analytic(d,MSKRTD_NZ,MSKRTD_NFFT1,state%cdat,pcoeffs,bvar)  

! Calculate average power for each frame and for the entire block.
! If decode is successful, largest power will be taken as signal+noise.
! If no decode, entire-block average will be used to update noise estimate.
  pmax=-99
  do i=1,8 
     ib=(i-1)*NSPM+1
     ie=ib+NSPM-1
     pow(i)=real(dot_product(state%cdat(ib:ie),state%cdat(ib:ie)))*rms**2
     pmax=max(pmax,pow(i))
  enddo
  pavg=sum(pow)/8.0

! Short ping decoder uses squared-signal spectrum to determine where to
! center a 3-frame analysis window and attempts to decode each of the 
! 3 frames along with 2- and 3-frame averages. 
  np=8*NSPM
  call msk144spd(state%cdat,np,ntol,ndecodesuccess,msgreceived,fc,fest,tdec,navg,ct, &
                 softbits)
  bshdecode=.false.
  if(ndecodesuccess.eq.0 .and. (bshmsg.or.bswl)) then
     call msk40spd(state%cdat,np,ntol,mycall,hiscall,bswl,state%recent_calls, &
              state%nhasharray,ndecodesuccess,msgreceived,fc,fest,tdec,navg)
     if(ndecodesuccess .ge.1) bshdecode=.true.
  endif
  if( ndecodesuccess .ge. 1 ) then
    tdec=tsec+tdec
    ipk=0
    is=0
    goto 900
  endif 

! If short ping decoder doesn't find a decode, 
! Fast - short ping decoder only. 
! Normal - try 4-frame averages
! Deep - try 4-, 5- and 7-frame averages. 
  npat=MSKRTD_NPATTERNS
  if( ndepth .eq. 1 ) npat=0
  if( ndepth .eq. 2 ) npat=2
  do iavg=1,npat
     iavmask=iavpatterns(1:8,iavg)
     navg=sum(iavmask)
     deltaf=10.0/real(navg)  ! search increment for frequency sync
     npeaks=2
     call msk144sync(state%cdat(1:8*NSPM),8,ntol,deltaf,iavmask,npeaks,fc,     &
          fest,npkloc,nsyncsuccess,xmax,c)
     if( nsyncsuccess .eq. 0 ) cycle

     do ipk=1,npeaks
        do is=1,3   
           ic0=npkloc(ipk)
           if(is.eq.2) ic0=max(1,ic0-1)
           if(is.eq.3) ic0=min(NSPM,ic0+1)
           ct=cshift(c,ic0-1)
           call msk144decodeframe(ct,softbits,msgreceived,ndecodesuccess)
           if(ndecodesuccess .gt. 0) then
              tdec=tsec+xmc(iavg)*tframe
              goto 900
           endif
        enddo                         !Slicer dither
     enddo                            !Peak loop 
  enddo


  msgreceived=' '

! no decode - update noise level used for calculating displayed snr.  
  if( state%pnoise .lt. 0 ) then         ! initialize noise level
     state%pnoise=pavg
  elseif( pavg .gt. state%pnoise ) then  ! noise level is slow to rise
     state%pnoise=0.9*state%pnoise+0.1*pavg
  elseif( pavg .lt. state%pnoise ) then  ! and quick to fall
     state%pnoise=pavg
  endif
  go to 999

900 continue
! Successful decode - estimate snr 
  if( state%pnoise .gt. 0.0 ) then
    snr0=10.0*log10(pmax/state%pnoise-1.0)
  else
    snr0=0.0
  endif
  nsnr=nint(snr0)

  if(.not. bshdecode) then
    call msk144signalquality(ct,snr0,fest,tdec,softbits,msgreceived,hiscall,   &
                          btrain,datadir,ncorrected,eyeopening,pcoeffs)
  endif

  decsym=' &  '
  if( btrain ) decsym=' ^  '
  if( bshdecode ) then
    ncorrected=0
    eyeopening=0.0
  endif

  if( nsnr .lt. -8 ) nsnr=-8
  if( nsnr .gt. 24 ) nsnr=24

! Dupe check. 
  bflag=ndecodesuccess.eq.1 .and.                                              &
        (msgreceived.ne.state%msglast .or. nsnr.gt.state%nsnrlast .or. tsec.lt.state%tsec0)
  if(bflag) then
     state%msglast=msgreceived
     state%nsnrlast=nsnr
     if(.not. bshdecode) then
        call update_msk40_hasharray(state%recent_calls,state%nhasharray)
     endif
     write(line,1021) nutc0,nsnr,tdec,nint(fest),decsym,msgreceived,char(0)
1021 format(i6.6,i4,f5.1,i5,a4,a37,a1)
  elseif(bswl .and. ndecodesuccess.ge.2) then 
    seenb4=.false.
    do i=1,MSKRTD_NSHMEM
      if( msgreceived .eq. state%recent_shmsgs(i) ) then
        seenb4=.true.
      endif
    enddo
    call update_recent_shmsgs(msgreceived,state%recent_shmsgs,MSKRTD_NSHMEM)
    bflag=seenb4 .and.                                                        &
      (msgreceived.ne.state%msglastswl .or. nsnr.gt.state%nsnrlastswl .or. tsec.lt.state%tsec0) & 
      .and. nsnr.gt.-6
    if(bflag) then
      state%msglastswl=msgreceived
      state%nsnrlastswl=nsnr
      write(line,1021) nutc0,nsnr,tdec,nint(fest),decsym,msgreceived,char(0)
    endif
  endif
999 state%tsec0=tsec

  return
end subroutine mskrtd

subroutine update_recent_shmsgs(message,msgs,nsize)
  character*37 msgs(nsize)
  character*37 message
  logical*1 seen

  seen=.false.
  do i=1,nsize
    if( msgs(i) .eq. message ) seen=.true. 
  enddo

  if( .not. seen ) then
    do i=nsize,2,-1
      msgs(i)=msgs(i-1)
    enddo
    msgs(1)=message
  endif

  return
end subroutine update_recent_shmsgs
