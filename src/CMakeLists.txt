file (GLOB SOURCES
  common.cpp
  logging.cpp
  main.cpp
)

target_sources(libommpfritt PRIVATE ${SOURCES})

add_subdirectory(aspects)
add_subdirectory(color)
add_subdirectory(commands)
add_subdirectory(geometry)
add_subdirectory(keybindings)
add_subdirectory(mainwindow)
add_subdirectory(managers)
add_subdirectory(objects)
add_subdirectory(properties)
add_subdirectory(propertywidgets)
add_subdirectory(python)
add_subdirectory(renderers)
add_subdirectory(scene)
add_subdirectory(serializers)
add_subdirectory(tags)
add_subdirectory(tools)
add_subdirectory(widgets)
