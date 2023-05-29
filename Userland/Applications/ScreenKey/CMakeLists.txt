serenity_component(
    ScreenKey
    REQUIRED
    TARGETS ScreenKey
)

set(SOURCES
    main.cpp
    ViewWidget.cpp
)

serenity_app(ScreenKey ICON filetype-executable)
target_link_libraries(ScreenKey PRIVATE LibC LibConfig LibCore LibGfx LibGUI LibMain LibThreading)
