subroutine libration(jd,RA,Dec,xl,b)

! Compute optical libration of moon at jd: that is, the sub-observer 
! point (xl,b) in selenographic coordinates.  RA and Dec are 
! topocentric values.

  implicit real*8 (a-h,o-z)
  parameter (RADS=0.0174532925199433d0)
  parameter (TWOPI=6.28318530717959d0)
  real*8 jd,j2000,mjd,lambda

  j2000=2451545.0d0
  RA2000=RA
  Dec2000=Dec
  year=2000.0d0 + (jd-j2000)/365.25d0
  mjd=jd-2400000.d0
  call sla_PRECES('FK5',year,2000.d0,RA2000,Dec2000)
  call sla_EQECL(RA2000,Dec2000,mjd,lambda,beta)
  day=jd - j2000
  t = day / 36525.d0
  xi = 1.54242 * RADS
  ft = 93.2720993 + 483202.0175273 * t - .0034029 * t * t
  b= ft / 360
  a = 360 * (b - floor(b))
  if (a.lt.0.d0) a = 360 + a;
  f=a/57.2957795131d0
  omega=sla_dranrm(2.182439196d0 - t*33.7570446085d0 + t*t*3.6236526d-5)
  w = lambda - omega
  y = sin(w) * cos(beta) * cos(xi) - sin(beta) * sin(xi)
  x = cos(w) * cos(beta)
  a = sla_dranrm(atan2(y, x))
  xl = a - f
  if(xl.lt.-0.25*TWOPI) xl=xl+TWOPI             !Fix 'round the back' angles
  if(xl.gt.0.25*TWOPI)  xl=xl-TWOPI
  b = asin(-sin(w) * cos(beta) * sin(xi) - sin(beta) * cos(xi))

  return
end subroutine libration
