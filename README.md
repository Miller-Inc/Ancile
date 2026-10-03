# Ancile
## About
This program is a simple GUI library that combines SDL with Vulkan and 
    ImGui. This allows for easy cross-platform GUI development with 
    a development stack ready for use. This project is currently being
    used as the base for a larger project, though is fully functional 
    with this simple setup. 

### Dependencies
The entire goal of this project is to have as little dependencies as 
    possible, and the ones that are required need to be as 
    cross-platform as I can get. Moreover, I wanted the dependencies 
    to also be open source and available for anyone to use, yet still
    familiar, in name at least, to the average developer. This is why
    I chose the following. 

 - [x] [ImGui](https://github.com/ocornut/imgui): A simple GUI library
        with a large selection of available backends for use. 
 - [x] [SDL3](https://www.libsdl.org/): A standard cross-platform media
        layer that allows platform specific window control on a variety
        of OSes and graphics configurations.
 - [x] [OpenGL Mathematics (glm)](https://github.com/g-truc/glm): A 
        simple, cross-platform game ready library to allow for quick 
        arithmatic operations
 - [x] [Vulkan](https://vulkan.org/): A game ready, cross-platform
        graphics backend that allows the ImGui and SDL layers to run
        on most devices available (as opposed to DirectX or other
        proprietary graphics backends).

### How it Works
This template is, in itself, a fully working app, though it does 
    absolutely nothing of value. What it does is provide a basis
    where all the library configuring is already done. The 
    `main.cpp` file located in the `src` directory allows you, as 
    the developer to design and create your app's runtime logic
    with little overhead. Then it will pass over the control to the 
    frontend to allow user input while still having a thread 
    available for backend processing. 

The frontend is controlled in the `App.h` and `App.cpp` files. These
    files define the frontend (GUI) `Application` class as well as an
    important virtual class called `Process`. The `Process` class is 
    meant to provide a universal way to connect with the application
    layer to provide a predetermined structure to how the program runs,
    allowing for modularity. `Application` also setting up a new thread 
    that takes over as the `Main` thread that we are all used to, while
    also keeping a reference to the current `Application` instance handy.

In `main.cpp`, as with every normal C++ program, the runtime starts, 
    allowing the developer to define some key features to their program.
    This includes setting up some of the main features as to how the 
    window loads, which windows to load, what "background" processes you 
    want to run, as well as what you want to continue on your main thread.
    Please note that all rendering has to be done on the actual main
    thread and not the secondary (`Application` controlled) thread. 

You may ask, what do I mean by "rendering" and "main" thread. This is a
    concerning discrepancy from what on would expect, and that is because
    I have chosen the unfortunate naming convention. One that states that
    the `main` function is the function where the work actually gets done
    and the `Run` (render thread) is where the rendering is done. Now you 
    may notice that in reality, the real execution path in the `Application`
    instance shows that the rendering thread IS the main thread, and that 
    would be technically correct. But that said, by calling it the render
    thread, I am basically saying that the new thread spawned when `Run()`
    is called is the new "main" thread. This is done to make sure that 
    the rendering happens on the primary thread since that is required
    by the graphics pipeline to happen (as in programs like game engines 
    where rendering is on one thread). 


The other thing of note here is that the current CMakeLists.txt should be
    enough to just start writing so long as you make sure to add your custom
    `.cpp` files to the `src/CMakeLists.txt`. This allows for easy editing 
    and customization while keeping the files that the developer has to 
    interface short and sweet. I would suggest to create subdirectories in the 
    `include` and `src` directories to organize your project specific window
    code. I had it setup to have a `/Wins` and `/Procs` directories to store
    windows and processes (non-gui) code respectively. 

## How to Use
To create a new project using this template, the primary work will be done 
    in new subclasses that you decide to make, so you must first make `Process`es.
    the `Process` virtual class is defined in the `App.h` file and should be all 
    you need to have a simple window or background process. To be completely honest, 
    the "background" processes are not actually background, as they run on the same
    thread, but they are just not shown as the window processes are. To have true
    multithreaded performance, you must create and assign a `Main` method to the 
    `Application` instance. An example of this would be to create a `Process` called
    `Testing` in say, `src/Wins/Testing.cpp` and the header in `include/Wins/Testing.h`
    and add to the end of the list of files in `src/CMakeLists.txt`, `Wins/Testing.cpp`.
    Then you can use the following `main.cpp`

~~~c++
#include <iostream>
#include "App.h"
// Replace with whatever window type you made
#include "Wins/Testing.h"

int main(const int argc, char ** argv) {
    // Creates the application insance
    Ancile::Application app{"Example", 750, 500};
    // Creates the window that you want to have the 
    //    app run for you
    const auto tWindow = std::make_shared<Testing>();
    // Add the window to the app so that when it starts,
    //    it can immediately start displaying the window
    app.AddProcess(tWindow);
    
    // You can add however many processes you want, just know
    //     that for each frame that has to be rendered, each
    //     window needs to be also rendered. This may not be
    //     the most efficent way of doing it, but it is a quick
    //     and easy way. 
    
    // Create a new "Main" function that will take a reference to your app on startup,
    //    notice that it also take your command line arguments. This means that
    //    if you wanted to parse your arguments somewhere else, your could do that here
    app->SetControlThread([](Ancile::Application* appRef, const int argc, char** argv) {
        printf("Running \"MAIN\"\n");
        // In this function, you can put whatever you want so long as it would run in 
        //    your normal main function (literally anything except rendering code)
    });
    // Actually start the application, this starts a new 
    //    thread that runs your "Main" on it
    app.Run(argc, argv);
    return 0;
}
~~~