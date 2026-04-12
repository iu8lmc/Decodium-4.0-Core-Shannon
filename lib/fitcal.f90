subroutine fitcal(x,y,r,iz,a,b,sigmaa,sigmab,rms)
  implicit real*8 (a-h,o-z)
  real*8 x(iz),y(iz),r(iz)

  sx=0.d0
  sy=0.d0
  sxy=0.d0
  sx2=0.d0
  do i=1,iz
     sx=sx + x(i)
     sy=sy + y(i)
     sxy=sxy + x(i)*y(i)
     sx2=sx2 + x(i)*x(i)
  enddo
  delta=iz*sx2 - sx*sx
  a=(sx2*sy - sx*sxy)/delta
  b=(iz*sxy - sx*sy)/delta

  sq=0.d0
  do i=1,iz
     r(i)=y(i) - (a + b*x(i))
     sq=sq + r(i)**2
  enddo
  rms=0.
  sigmaa=0.
  sigmab=0.
  if(iz.ge.3) then
     rms=sqrt(sq/(iz-2))
     sigmaa=sqrt(rms*rms*sx2/delta)
     sigmab=sqrt(iz*rms*rms/delta)
  endif

  return
end subroutine fitcal
