#include "common.h"
#include "maths.h"
#include "renderer.c"

#include "core.c"

#include <time.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Carbon/Carbon.h>

#include <IOKit/graphics/IOGraphicsLib.h>
#include <CoreVideo/CVBase.h>

#define MAX_VERTS 65536

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (assign) IBOutlet NSWindow *window;
@end

@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}
@end

@interface MetalViewController : NSViewController<MTKViewDelegate>
@end

@interface MetalView : MTKView
@property struct Input *input;
@property struct Memory *memory;
@end

@implementation MetalView
@synthesize input;
@synthesize memory;

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:_bounds
            options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow )
            owner:self userInfo:nil];
        [self addTrackingArea:trackingArea];
    }

    return self;
}

- (void)keyDown:(NSEvent *)event {
    switch (event.keyCode) {
        case kVK_Escape:     input->keys[K_Escape] = true; break;
        case kVK_UpArrow:    input->keys[K_Up] =     true; break;
        case kVK_DownArrow:  input->keys[K_Down] =   true; break;
        case kVK_LeftArrow:  input->keys[K_Left] =   true; break;
        case kVK_RightArrow: input->keys[K_Right] =  true; break;
    }
}

- (void)keyUp:(NSEvent *)event {
    switch (event.keyCode) {
        case kVK_Escape:     input->keys[K_Escape] = false; break;
        case kVK_UpArrow:    input->keys[K_Up] =     false; break;
        case kVK_DownArrow:  input->keys[K_Down] =   false; break;
        case kVK_LeftArrow:  input->keys[K_Left] =   false; break;
        case kVK_RightArrow: input->keys[K_Right] =  false; break;
    }
}

- (void)magnifyWithEvent:(NSEvent *)event {
    input->zoomX = (f32)[event magnification];
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint pos = [event locationInWindow];
    input->mouseX = (f32)pos.x;
    input->mouseY = (f32)(self.bounds.size.height - pos.y - 1);
}

- (void)mouseUp:(NSEvent *)event {
    if (input->isDragging) {
        input->wasDragging = true;
    }
    input->isDragging = false;
    input->mouseButtons[MB_Left] = false;
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint pos = [event locationInWindow];
    input->mouseX = (f32)pos.x;
    input->mouseY = (f32)(self.bounds.size.height - pos.y - 1);
    input->mouseButtons[MB_Left] = true;

    if (!input->isDragging) {
        input->dragStartX = input->mouseX;
        input->dragStartY = input->mouseY;
    }

    input->isDragging = true;
}

- (void)rightMouseDragged:(NSEvent *)event {
    NSPoint pos = [event locationInWindow];
    input->mouseX = (f32)pos.x;
    input->mouseY = (f32)(self.bounds.size.height - pos.y - 1);
    input->mouseButtons[MB_Right] = true;
}

- (void)scrollWheel:(NSEvent *)event {
    input->scrollX = -[event scrollingDeltaX];
    input->scrollY = [event scrollingDeltaY];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}
@end

@implementation MetalViewController {
    MetalView *_view;
    id<MTLDevice> _device;

    id<MTLRenderPipelineState> _coloredQuadState;
    id<MTLRenderPipelineState> _texturedQuadState;

    id<MTLBuffer> _vertBuffer;
    id<MTLBuffer> _texturedVertBuffer;

    id<MTLTexture> _fontTexture;
    stbtt_packedchar _headerChars[96];
    stbtt_packedchar _textChars[96];

    id<MTLCommandQueue> _commandQueue;
    vector_uint2 _viewportSize;
    uint _scaleFactor;

    struct State *_state;
    struct Input *_input;
    struct Memory *_memory;
    struct Time *_time;
    usTimer _timer;

    u64 _lastTick;
    u64 _startup;
    u64 _refreshRate;
}

void *mapMemory(void *start, u64 size) {
    void *mem = mmap(start, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == (void *)-1) {
        NSLog(@"Failed to allocate memory!");
        [[NSApplication sharedApplication] terminate:NULL];
    }

    return mem;
}

- (NSData *)loadFontWithName:(NSString *)name andType:(NSString *)type {
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"org.brettrtoomey.mozarello"];
    NSString *path = [bundle pathForResource:name ofType:type];
    NSData *data = [NSData dataWithContentsOfFile:path];
    return data;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    NSError *error = NULL;

    CGDirectDisplayID displayId = CGMainDisplayID();
    CGDisplayModeRef displayInfo = CGDisplayCopyDisplayMode(displayId);
    u64 rate = (u64) CGDisplayModeGetRefreshRate(displayInfo);
    CVDisplayLinkRef link;
    CVDisplayLinkCreateWithCGDisplay(displayId, &link);

    if (rate == 0) {
        const CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
        if (!(time.flags & kCVTimeIsIndefinite)) {
            rate = (u64)(time.timeScale / (double)time.timeValue);
        }
    }

    _refreshRate = rate;
    _view = (MetalView *)self.view;

    _device = MTLCreateSystemDefaultDevice();
    _view.device = _device;

    if(!_view.device) {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.label = @"Pipeline";

    id<MTLLibrary> defaultLibrary = [_device newDefaultLibrary];
    
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.vertexFunction = [defaultLibrary newFunctionWithName:@"vertexShader"];
    pipelineDescriptor.fragmentFunction = [defaultLibrary newFunctionWithName:@"fragmentShader"];
    pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
    _coloredQuadState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                             error:&error];
    if (!_coloredQuadState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        [[NSApplication sharedApplication] terminate:self];
    }

    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.vertexFunction = [defaultLibrary newFunctionWithName:@"texturedVertShader"];
    pipelineDescriptor.fragmentFunction = [defaultLibrary newFunctionWithName:@"texturedFragmentShader"];
    _texturedQuadState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                 error:&error];

    if (!_texturedQuadState) {
        NSLog(@"Failed to created pipeline state, error %@", error);
        [[NSApplication sharedApplication] terminate:self];
    }

    _scaleFactor = (uint)(_view.drawableSize.width / self.view.frame.size.width);

    _state =  calloc(1, sizeof(struct State));
    _input =  calloc(1, sizeof(struct Input));
    _memory = calloc(1, sizeof(struct Memory));
    _time =   calloc(1, sizeof(struct Time));

    DefaultState(_state);

    _view.input =  _input;
    _view.memory = _memory;

    void *memStart = (void *)TB(1);
    void *mem = mapMemory(memStart, MB(10));

    _vertBuffer = [_device newBufferWithLength:MAX_VERTS * sizeof(struct Vert) options:MTLResourceStorageModeShared];
    _texturedVertBuffer = [_device newBufferWithLength:MAX_VERTS * sizeof(struct TexturedVert) options:MTLResourceStorageModeShared];

    _memory->buffer = mem;

    const unsigned char *fontData = [[self loadFontWithName:@"SF-Pro-Text-Regular" andType:@"otf"] bytes];
    unsigned char *fontBitmap = malloc(1024*1024);

    stbtt_pack_context context;
    stbtt_PackBegin(&context, fontBitmap, 1024, 1024, 0, 1, NULL);
    stbtt_PackSetOversampling(&context, 4, 4);
    stbtt_PackFontRange(&context, fontData, 0, 12.0, 32, 92, &_textChars[0]);
    stbtt_PackFontRange(&context, fontData, 0, 20.0, 32, 92, &_headerChars[0]);
    stbtt_PackEnd(&context);

    MTLTextureDescriptor *fontTextureDesc = [[MTLTextureDescriptor alloc] init];
    fontTextureDesc.pixelFormat = MTLPixelFormatR8Unorm;
    fontTextureDesc.width = 1024;
    fontTextureDesc.height = 1024;

    _fontTexture = [_device newTextureWithDescriptor:fontTextureDesc];
    MTLRegion region = {
        {0, 0, 0},
        {1024, 1024, 1}
    };
    [_fontTexture replaceRegion:region mipmapLevel:0 withBytes:fontBitmap bytesPerRow:1024*1];
    free(fontBitmap);

    usTimerInit(&_timer);
    _lastTick = _startup = GetTimeus(&_timer);
    _commandQueue = [_device newCommandQueue];
    _viewportSize.x = _view.drawableSize.width;
    _viewportSize.y = _view.drawableSize.height;
    _view.delegate = self;
}

- (void)renderFrame:(nonnull MTKView *)view {
    b32 isRunning = true;
    u64 now = GetTimeus(&_timer);
    _time->dt = (f64)(now - _lastTick) / 1000000.0;
    _time->global = (f64)(now - _startup) / 1000000.0;

    struct RenderCommands renderCommands = RenderCommandsInit(
        MB(1),
        _memory->buffer,
        MAX_VERTS,
        _vertBuffer.contents,
        MAX_VERTS,
        _texturedVertBuffer.contents,
        (u32)_view.frame.size.width,
        (u32)_view.frame.size.height
    );

    struct Font headerFont = {
        (__bridge void *)_fontTexture,
        &_headerChars[0]
    };

    struct Font textFont = {
        (__bridge void *)_fontTexture,
        &_textChars[0]
    };

    renderCommands.settings.headerFont = &headerFont;
    renderCommands.settings.textFont = &textFont;

    Tick(_state, _time, _input, _memory, &renderCommands, &isRunning);

    simd_float2 halfView = simd_make_float2(_viewportSize.x, _viewportSize.y) / 2.0;
    simd_float4x2 m;
    m.columns[0].x = (1.0 / halfView.x) * _scaleFactor;
    m.columns[0].y = 0.0;

    m.columns[1].x =  0.0;
    m.columns[1].y = (-1.0 / halfView.y) * _scaleFactor;

    m.columns[2].x = -1.0;
    m.columns[2].y =  0.0;

    m.columns[3].x = 0.0;
    m.columns[3].y = 1.0;

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"Command Buffer";

    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;

    if(renderPassDescriptor != nil)
    {
        id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"Render Encoder";

        [renderEncoder setViewport:(MTLViewport){0.0, 0.0, _viewportSize.x, _viewportSize.y, -1.0, 1.0 }];

        for (
             u8 *headerIndex = renderCommands.commandBuffer;
             headerIndex < renderCommands.commandBuffer + renderCommands.commandIndex;
             headerIndex += sizeof(struct RenderEntryHeader)
        ) {
            struct RenderEntryHeader *header = (struct RenderEntryHeader *)headerIndex;
            void *payload = (u8 *)header + sizeof(struct RenderEntryHeader);

            switch (header->type) {
                case RenderEntryType_Clear: {
                    headerIndex += sizeof(struct RenderEntryClear);
                    struct RenderEntryClear *entry = (struct RenderEntryClear *)payload;
                    v4 color = entry->clearColor;
                    _view.clearColor = MTLClearColorMake(
                        color.r,
                        color.g,
                        color.b,
                        color.a
                    );
                } break;

                case RenderEntryType_Quads: {
                    headerIndex += sizeof(struct RenderEntryQuads);
                    struct RenderEntryQuads *entry = (struct RenderEntryQuads *)payload;
                    [renderEncoder setVertexBuffer:_vertBuffer offset:0 atIndex:0];
                    [renderEncoder setVertexBytes:&m length:sizeof(m) atIndex:1];
                    [renderEncoder setVertexBytes:&entry->kind length:sizeof(enum QuadKind) atIndex:2];
                    [renderEncoder setRenderPipelineState:_coloredQuadState];
                    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:entry->vertexIndex vertexCount:entry->count * VERTS_PER_QUAD];
                } break;

                case RenderEntryType_TexturedQuads: {
                    headerIndex += sizeof(struct RenderEntryTexturedQuads);
                    struct RenderEntryTexturedQuads *entry = (struct RenderEntryTexturedQuads *)payload;
                    [renderEncoder setVertexBuffer:_texturedVertBuffer offset:0 atIndex:0];
                    [renderEncoder setVertexBytes:&m length:sizeof(m) atIndex:1];
                    [renderEncoder setFragmentTexture:_fontTexture atIndex:0];
                    [renderEncoder setFragmentBytes:&entry->color length:sizeof(entry->color) atIndex:0];
                    [renderEncoder setRenderPipelineState:_texturedQuadState];
                    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:entry->vertexIndex vertexCount:entry->count * VERTS_PER_QUAD];
                } break;
            }
        }

        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];

    _lastTick = now;

    if (!isRunning) {
        [[NSApplication sharedApplication] terminate:self];
    }
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    @autoreleasepool {
        [self renderFrame:view];
    }
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    _viewportSize.x = size.width;
    _viewportSize.y = size.height;
    uint scale = (uint)(size.width / self.view.frame.size.width);
    if (scale != _scaleFactor) {
        _scaleFactor = scale;
    }
}

- (void)encodeWithCoder:(nonnull NSCoder *)aCoder {}
@end

int main(int argc, const char * argv[]) {
    return NSApplicationMain(argc, argv);
}
