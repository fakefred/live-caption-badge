file(GLOB EPAPER_C_SRC epaper/*.c)

set(
	COMPONENT_SRCS
	main.c
	audio.c
	http.c
	${EPAPER_C_SRC}
)
set(COMPONENT_ADD_INCLUDEDIRS "")
set(COMPONENT_EMBED_TXTFILES)

register_component()
