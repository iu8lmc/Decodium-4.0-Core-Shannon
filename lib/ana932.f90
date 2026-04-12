subroutine ana932(dat,npts0,cdat,npts)

  real dat(npts0)
  complex cdat(262145)

  n=log(float(npts0))/log(2.0)
  nfft1=2**(n+1)
  nfft2=9*nfft1/32
  df932=11025.0/nfft1
  fac=2.0/nfft1
  do i=1,npts0/2
     cdat(i)=fac*cmplx(dat(2*i-1),dat(2*i))
  enddo
  cdat(npts0/2+1:nfft1/2)=0.
  call four2a(cdat,nfft1,1,-1,0)               !Forward r2c FFT
  call four2a(cdat,nfft2,1,1,1)                !Inverse c2c FFT
  npts=npts0*9.0/32.0                          !Downsampled data length
  npts2=npts

  return
end subroutine ana932
