subroutine normalizebmet(bmet,n)
  real bmet(n)

  bmetav=sum(bmet)/real(n)
  bmet2av=sum(bmet*bmet)/real(n)
  var=bmet2av-bmetav*bmetav
  if(var.gt.0.0) then
     bmetsig=sqrt(var)
  else
     bmetsig=sqrt(bmet2av)
  endif
  bmet=bmet/bmetsig
  return
end subroutine normalizebmet
