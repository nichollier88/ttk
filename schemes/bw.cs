\name B&W Only
\def black  #000000
\def white  #ffffff

  header: bg => white, fg => black, line => black -1, accent => white
	  shadow => black, shine => white,
	  gradient.top => white,
	  gradient.middle => white,
	  gradient.bottom => black,
	  gradient.bar => white
   music: bar => black
 battery: border => black, bg => white, fill.normal => black +1, fill.low => black +1, fill.charge => black +1,
	  bg.low => white, bg.charging => white, chargingbolt => black
    lock: border => black, fill => black
 loadavg: bg => white, fg => white, spike => black

  window: bg => white, fg => black, border => white -3
  dialog: bg => white, fg => black, line => black,
          title.fg => black,	
          button.bg => white, button.fg => black, button.border => white,
	  button.sel.bg => white, button.sel.fg => black, button.sel.border => black, button.sel.inner => black +1
   error: bg => white, fg => black, line => black,
          title.fg => black,
          button.bg => white, button.fg => black, button.border => black,
          button.sel.bg => black, button.sel.fg => white, button.sel.border => black, button.sel.inner => white +1
  scroll: box => black, bg => white +1, bar => black +2
   input: bg => white, fg => black, selbg => black, selfg => white, border => white, cursor => black

    menu: bg => white, fg => black, choice => black, icon => black,
	  hdrfg => white, hdrbg => white,
          selbg => black, selfg => white, selchoice => white, icon0 => black,
          icon0 => black, icon1 => black, icon2 => white, icon3 => white
  slider: border => black, bg => white, full => black
textarea: bg => white, fg => black

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => white,
	default.fg => black,
	default.border => white,
	selected.bg => black,
	selected.fg => white,
	selected.border => white,
	special.bg => white,
	special.fg => black,
	special.border => black

button:
	default.bg => white,
	default.fg => black,
	default.border => black,
	selected.bg => black,
	selected.fg => white,
	selected.border => black
