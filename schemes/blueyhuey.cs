\name Bluey Huey
\def blue   #273db5
\def white  #dcdfe4

  header: bg => white, fg => blue, line => blue -1, accent => white
	  shadow => blue, shine => white,
	  gradient.top => white,
	  gradient.middle => white,
	  gradient.bottom => blue,
	  gradient.bar => white
   music: bar => blue
 battery: border => blue, bg => white, fill.normal => blue +1, fill.low => blue +1, fill.charge => blue +1,
	  bg.low => white, bg.charging => white, chargingbolt => blue
    lock: border => blue, fill => blue
 loadavg: bg => white, fg => white, spike => blue

  window: bg => white, fg => blue, border => white -3
  dialog: bg => white, fg => blue, line => blue,
          title.fg => blue,	
          button.bg => white, button.fg => blue, button.border => white,
	  button.sel.bg => white, button.sel.fg => blue, button.sel.border => blue, button.sel.inner => blue +1
   error: bg => white, fg => blue, line => blue,
          title.fg => blue,
          button.bg => white, button.fg => blue, button.border => blue,
          button.sel.bg => blue, button.sel.fg => white, button.sel.border => blue, button.sel.inner => white +1
  scroll: box => blue, bg => white +1, bar => blue +2
   input: bg => white, fg => blue, selbg => blue, selfg => white, border => white, cursor => blue

    menu: bg => white, fg => blue, choice => blue, icon => blue,
	  hdrfg => white, hdrbg => white,
          selbg => blue, selfg => white, selchoice => white, icon0 => blue,
          icon0 => blue, icon1 => blue, icon2 => white, icon3 => white
  slider: border => blue, bg => white, full => blue
textarea: bg => white, fg => blue

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => blue,
	default.border => white,
	selected.bg => blue,
	selected.fg => white,
	selected.border => white,
	special.bg => white,
	special.fg => blue,
	special.border => blue

button:
	default.bg => white,
	default.fg => blue,
	default.border => blue,
	selected.bg => blue,
	selected.fg => white,
	selected.border => blue
