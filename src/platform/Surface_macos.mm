#include "platform/Surface.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

namespace fb {

wgpu::Surface createSurface(const wgpu::Instance& instance, GLFWwindow* window) {
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    NSView* view = [nsWindow contentView];

    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.contentsScale = [nsWindow backingScaleFactor];
    [view setWantsLayer:YES];
    [view setLayer:layer];

    wgpu::SurfaceSourceMetalLayer source;
    source.layer = (__bridge void*)layer;

    wgpu::SurfaceDescriptor desc;
    desc.nextInChain = &source;
    return instance.CreateSurface(&desc);
}

} // namespace fb
