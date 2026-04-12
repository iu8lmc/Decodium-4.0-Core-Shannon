subroutine pltanh(x,y)
  isign=+1
  z=x
  if( x.lt.0 ) then
    isign=-1
    z=abs(x)
  endif
  if( z.le. 0.8 ) then
    y=0.83*x
    return
  elseif( z.le. 1.6 ) then
    y=isign*(0.322*z+0.4064)
    return  
  elseif( z.le. 3.0 ) then
    y=isign*(0.0524*z+0.8378)
    return
  elseif( z.lt. 7.0 ) then
    y=isign*(0.0012*z+0.9914)
    return
  else
    y=isign*0.9998
    return
  endif
end subroutine pltanh
