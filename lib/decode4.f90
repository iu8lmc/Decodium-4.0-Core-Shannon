subroutine decode4(dat,npts,dtx,nfreq,flip,mode4,ndepth,neme,minw,           &
     mycall,hiscall,hisgrid,decoded,nfano,deepbest,qbest,ichbest,           &
     rsymbol_out,ich1_out,ich2_out)

! Decodes JT4 data, assuming that DT and DF have already been determined.
! Input dat(npts) has already been downsampled by 2: rate = 11025/2.
! ### NB: this initial downsampling should be removed in WSJT-X, since
! it restricts the useful bandwidth to < 2.7 kHz.

  use jt4, only: nch, npr
  real dat(npts)                        !Raw data
  character decoded*22,deepmsg*22,deepbest*22
  character*12 mycall,hiscall
  character*6 hisgrid
  real, intent(out) :: rsymbol_out(207,7)
  integer, intent(out) :: ich1_out, ich2_out
  real*8 dt,df,phi,f0,dphi,twopi,phi1,dphi1
  complex*16 cz,cz1,c0,c1
  real*4 sym(207)

  twopi=8*atan(1.d0)
  dt=2.d0/11025             !Sample interval (2x downsampled data)
  df=11025.d0/2520.d0       !Tone separation for JT4A mode
  nsym=206
  amp=15.0
  istart=nint((dtx+0.8)/dt)              !Start index for synced FFTs
  if(istart.lt.0) istart=0
  nchips=0
  qbest=0.
  qtop=0.
  deepmsg='                      '
  ichbest=-1
  c0=0.
  k=istart
  phi=0.d0
  phi1=0.d0

  rsymbol_out=0.0
  ich1_out=minw+1
  ich2_out=ich1_out
  do ich=1,7
     if(nch(ich).le.mode4) ich2_out=ich
  enddo

  do ich=ich1_out,ich2_out
     nchips=min(nch(ich),70)
     nspchip=1260/nchips
     k=istart
     phi=0.d0
     phi1=0.d0
     fac2=1.e-8 * sqrt(float(mode4))
     do j=1,nsym+1
        if(flip.gt.0.0) then
           f0=nfreq + (npr(j))*mode4*df
           f1=nfreq + (2+npr(j))*mode4*df
        else
           f0=nfreq + (1-npr(j))*mode4*df
           f1=nfreq + (3-npr(j))*mode4*df
        endif
        dphi=twopi*dt*f0
        dphi1=twopi*dt*f1
        sq0=0.
        sq1=0.
        do nc=1,nchips
           phi=0.d0
           phi1=0.d0
           c0=0.
           c1=0.
           do i=1,nspchip
              k=k+1
              phi=phi+dphi
              phi1=phi1+dphi1
              cz=dcmplx(cos(phi),-sin(phi))
              cz1=dcmplx(cos(phi1),-sin(phi1))
              if(k.le.npts) then
                 c0=c0 + dat(k)*cz
                 c1=c1 + dat(k)*cz1
              endif
           enddo
           sq0=sq0 + real(c0)**2 + aimag(c0)**2
           sq1=sq1 + real(c1)**2 + aimag(c1)**2
        enddo
        sq0=fac2*sq0
        sq1=fac2*sq1
        rsym=amp*(sq1-sq0)
        if(j.ge.1) then
           rsymbol_out(j,ich)=rsym
           sym(j)=rsym
        endif
     enddo
     
     call extract4(sym,ncount,decoded)          !Do the convolutional decode
     nfano=0
     if(ncount.ge.0) then
        nfano=1
        ichbest=ich
        exit
     endif

     qual=0.                                    !Now try deep search
!     if(ndepth.ge.1) then
     if(iand(ndepth,32).eq.32) then
        call deep4(sym(2),neme,flip,mycall,hiscall,hisgrid,deepmsg,qual)
        if(qual.gt.qbest) then
           qbest=qual
           deepbest=deepmsg
           ichbest=ich
        endif
     endif
  enddo
  if(qbest.gt.qtop) then
     qtop=qbest
  endif
  qual=qbest

  return
end subroutine decode4
