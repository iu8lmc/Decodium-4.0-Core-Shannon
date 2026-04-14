subroutine deep4(sym0,neme,flip,mycall,hiscall,hisgrid,decoded,qual)

! Deep search routine for JT4

  use prog_args
  parameter (MAXCALLS=7000,MAXRPT=63)
  real*4 sym0(206),sym(206)
  character callsign*12,grid*4,message*22,hisgrid*6,ceme*3
  character*12 mycall,hiscall
  character mycall0*12,hiscall0*12,hisgrid0*6
  character*22 decoded
  character*22 testmsg(2*MAXCALLS + 2 + MAXRPT)
  character*15, allocatable :: callgrid(:)
  character*180 line
  character*4 rpt(MAXRPT)
  integer ncode(206)
  real*4   code(206,2*MAXCALLS + 2 + MAXRPT)
  real pp(2*MAXCALLS + 2 + MAXRPT)
  data neme0/-99/
  data rpt/'-01','-02','-03','-04','-05',          &
           '-06','-07','-08','-09','-10',          &
           '-11','-12','-13','-14','-15',          &
           '-16','-17','-18','-19','-20',          &
           '-21','-22','-23','-24','-25',          &
           '-26','-27','-28','-29','-30',          &
           'R-01','R-02','R-03','R-04','R-05',     &
           'R-06','R-07','R-08','R-09','R-10',     &
           'R-11','R-12','R-13','R-14','R-15',     &
           'R-16','R-17','R-18','R-19','R-20',     &
           'R-21','R-22','R-23','R-24','R-25',     &
           'R-26','R-27','R-28','R-29','R-30',     &
           'RO','RRR','73'/
  save mycall0,hiscall0,hisgrid0,neme0,ntot,code,testmsg

  allocate(callgrid(MAXCALLS))

  sym=sym0
  if(mycall.eq.mycall0 .and. hiscall.eq.hiscall0 .and.         &
       hisgrid.eq.hisgrid0 .and. neme.eq.neme0) go to 30

  open(23,file=trim(data_dir)//'/CALL3.TXT',status='unknown')
  k=0
  icall=0
  do n=1,MAXCALLS
     if(n.eq.1) then
        callsign=hiscall
        do i=4,12
           if(ichar(callsign(i:i)).eq.0) callsign(i:i)=' '
        enddo
        grid=hisgrid(1:4)
        if(ichar(grid(3:3)).eq.0) grid(3:3)=' '
        if(ichar(grid(4:4)).eq.0) grid(4:4)=' '
     else
        read(23,1002,end=20) line
1002    format (A80)
        if(line(1:4).eq.'ZZZZ') go to 20
        if(line(1:2).eq.'//') go to 10
        i1=index(line,',')
        if(i1.lt.4) go to 10
        i2=index(line(i1+1:),',')
        if(i2.lt.5) go to 10
        i2=i2+i1
        i3=index(line(i2+1:),',')
        if(i3.lt.1) i3=index(line(i2+1:),' ')
        i3=i2+i3
        callsign=line(1:i1-1)
        grid=line(i1+1:i2-1)
        ceme=line(i2+1:i3-1)
        if(neme.eq.1 .and. ceme.ne.'EME') go to 10
     endif

     icall=icall+1
     j1=index(mycall,' ') - 1
     if(j1.le.-1) j1=12
     if(j1.lt.3) j1=6
     j2=index(callsign,' ') - 1
     if(j2.le.-1) j2=12
     if(j2.lt.3) j2=6
     j3=index(mycall,'/')                 ! j3>0 means compound mycall
     j4=index(callsign,'/')               ! j4>0 means compound hiscall
     callgrid(icall)=callsign(1:j2)

     mz=1
! Allow MyCall + HisCall + rpt (?)
     if(n.eq.1 .and. j3.lt.1 .and. j4.lt.1 .and. callsign(1:6).ne.'      ') &
          mz=MAXRPT+1
     do m=1,mz
        if(m.gt.1) grid=rpt(m-1)
        if(j3.lt.1 .and.j4.lt.1) callgrid(icall)=callsign(1:j2)//' '//grid
        message=mycall(1:j1)//' '//callgrid(icall)
        k=k+1
        testmsg(k)=message
        call encode4(message,ncode)
        code(1:206,k)=2*ncode(1:206)-1
        if(n.ge.2) then
! Insert CQ message
           if(j4.lt.1) callgrid(icall)=callsign(1:j2)//' '//grid
           message='CQ '//callgrid(icall)
           k=k+1
           testmsg(k)=message
           call encode4(message,ncode)
        code(1:206,k)=2*ncode(1:206)-1
        endif
     enddo
10   continue
  enddo

20 continue
  close(23)
  ntot=k

30 mycall0=mycall
  hiscall0=hiscall
  hisgrid0=hisgrid
  neme0=neme

  sq=0.
  do j=1,206
     sq=sq + sym(j)**2
  enddo
  rms=sqrt(sq/206.0)
  sym=sym/rms

  p1=-1.e30
  p2=-1.e30
  do k=1,ntot
     pp(k)=0.
! Should re-instate the following:
!     if(k.ge.2 .and. k.le.64 .and. flip.gt.0.0) cycle   
! Test all messages if flip=+1; skip the CQ messages if flip=-1.
     if(flip.gt.0.0 .or. testmsg(k)(1:3).ne.'CQ ') then
        p=0.
        do j=1,206
           p=p + code(j,k)*sym(j)
        enddo
        pp(k)=p
        if(p.gt.p1) then
           p1=p
           ip1=k
        endif
     endif
  enddo

  do i=1,ntot
     if(pp(i).gt.p2 .and. testmsg(i).ne.testmsg(ip1)) p2=pp(i)
  enddo

  qual=p1-max(1.15*p2,70.0)

! ### DO NOT REMOVE ### 
  rewind 77
  if(ip1.ge.1 .and. ip1.le.2*MAXCALLS+2+MAXRPT) write(77,1001) p1,p2,ntot,   &
       rms,qual,ip1,testmsg(ip1)
1001 format(2f8.2,i8,2f8.2,i6,2x,a22)
  call flush(77)

  if(qual.gt.1.0) then
     decoded=testmsg(ip1)
  else
     decoded='                      '
     qual=0.
  endif

! Make sure everything is upper case.
  do i=1,22
     if(decoded(i:i).ge.'a' .and. decoded(i:i).le.'z')                &
          decoded(i:i)=char(ichar(decoded(i:i))-32)
  enddo

  return
end subroutine deep4
