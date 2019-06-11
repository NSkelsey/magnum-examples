/*
    This file is part of Magnum.

    Original authors — credit is appreciated but not required:

        2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019 —
            Vladimír Vondruš <mosra@centrum.cz>

    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or distribute
    this software, either in source code form or as a compiled binary, for any
    purpose, commercial or non-commercial, and by any means.

    In jurisdictions that recognize copyright laws, the author or authors of
    this software dedicate any and all copyright interest in the software to
    the public domain. We make this dedication for the benefit of the public
    at large and to the detriment of our heirs and successors. We intend this
    dedication to be an overt act of relinquishment in perpetuity of all
    present and future rights to this software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <ctime>
#include <iomanip>
#include <iostream>

#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Debug.h>

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Complex.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/Shaders/DistanceFieldVector.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/DistanceFieldGlyphCache.h>
#include <Magnum/Text/Renderer.h>

#include <Magnum/Shaders/VertexColor.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Scene.h>


namespace Magnum { namespace Examples {

using namespace Magnum::Math::Literals;

typedef Magnum::SceneGraph::Object<Magnum::SceneGraph::MatrixTransformation3D> Object3D;
typedef Magnum::SceneGraph::Scene<Magnum::SceneGraph::MatrixTransformation3D> Scene3D;

class ThingAble: public Object3D, SceneGraph::Drawable3D {
    public:
        explicit ThingAble(Shaders::VertexColor3D& shader, Object3D& parent, SceneGraph::DrawableGroup3D& drawables): Object3D{&parent}, SceneGraph::Drawable3D{*this, &drawables}, _shader(shader) {}

    private:
        virtual void draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {
            auto m = camera.projectionMatrix()*transformationMatrix;
            Utility::Debug{} << "drawable_tp\n" << m << "\n";
            _shader.setTransformationProjectionMatrix(m);
            _mesh.draw(_shader);
        }

        Shaders::VertexColor3D& _shader;
        GL::Mesh _mesh;
};

class TextDrawable: public Object3D, SceneGraph::Drawable3D {
    public:
        explicit TextDrawable(std::string msg, Containers::Pointer<Text::AbstractFont> &font, Text::DistanceFieldGlyphCache *cache, Shaders::DistanceFieldVector3D& shader, Object3D& parent, SceneGraph::DrawableGroup3D& drawables): Object3D{&parent}, SceneGraph::Drawable3D{*this, &drawables}, _shader(shader) {

            _text.reset(new Text::Renderer3D(*font.get(), *cache, 0.035f, Text::Alignment::TopRight));
            _text->reserve(40, GL::BufferUsage::DynamicDraw, GL::BufferUsage::StaticDraw);

             updateText();

             _shader.setColor(Color3::fromHsv({216.0_degf, 0.85f, 1.0f}))
                    .setOutlineColor(Color3{0.95f})
                    .setOutlineRange(0.45f, 0.35f);
        }

        void updateText() {
            std::stringstream ss;
            auto time = std::time(nullptr);
            ss << std::put_time(std::localtime(&time), "%F %T%z");

            std::cout << ss.str() << std::endl;
            _text->render(ss.str());
        }



    private:
        virtual void draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera) {

            auto m = camera.projectionMatrix()*transformationMatrix;
            Utility::Debug{} << "textdrawable_tp\n" << m << "\n";
            _shader.setTransformationProjectionMatrix(m)
                   .setSmoothness(0.025f/ transformationMatrix.uniformScaling());
            _text->mesh().draw(_shader);
        }

        Containers::Pointer<Text::Renderer3D> _text;
        Shaders::DistanceFieldVector3D& _shader;
};


class TextExample: public Platform::Application {
    public:
        explicit TextExample(const Arguments& arguments);

    private:
        void viewportEvent(ViewportEvent& event) override;
        void drawEvent() override;
        void mouseScrollEvent(MouseScrollEvent& event) override;
        void mouseMoveEvent(MouseMoveEvent& event) override;


        PluginManager::Manager<Text::AbstractFont> _manager;
        Containers::Pointer<Text::AbstractFont> _font;

        Text::DistanceFieldGlyphCache _cache;
        GL::Mesh _text;
        GL::Buffer _vertices, _indices;
        Shaders::DistanceFieldVector3D _shader;

        Object3D *_cameraRig, *_cameraObject;
        SceneGraph::Camera3D* _camera;
        Scene3D _scene;
        SceneGraph::DrawableGroup3D _drawables;

        Vector2i _previousMousePosition, _mousePressPosition;
        Object3D *obj;

        Matrix4 _transformation;
        Matrix4 _projection;
        TextDrawable *_dynamicText;

        GL::Mesh _mesh;
        Shaders::VertexColor3D _triang_shader;
        int cnt = 0;
};

TextExample::TextExample(const Arguments& arguments): Platform::Application{arguments, Configuration{}.setTitle("Magnum Text Example")}, _cache(Vector2i(2048), Vector2i(512), 22), _text{NoCreate} {

    /* Camera setup */
    (*(_cameraRig = new Object3D{&_scene}));
    (*(_cameraObject = new Object3D{_cameraRig}))
        .translate(Vector3::zAxis(5.0f));
    (_camera = new SceneGraph::Camera3D(*_cameraObject))
        ->setAspectRatioPolicy(SceneGraph::AspectRatioPolicy::Extend)
        .setProjectionMatrix(Matrix4::perspectiveProjection(35.0_degf, Vector2{windowSize()}.aspectRatio(), 0.01f, 100.0f))
        .setViewport(GL::defaultFramebuffer.viewport().size()); /* Drawing setup */

    /* Load FreeTypeFont plugin */
    _font = _manager.loadAndInstantiate("FreeTypeFont");
    if(!_font) std::exit(1);

    /* Open the font and fill glyph cache */
    Utility::Resource rs("fonts");
    if(!_font->openData(rs.getRaw("DejaVuSans.ttf"), 110.0f)) {
        Error() << "Cannot open font file";
        std::exit(1);
    }

    _font->fillGlyphCache(_cache, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-+,.!°ěäЗдравстуймиΓειασουκόμ ");
    std::tie(_text, std::ignore) = Text::Renderer3D::render(*_font, _cache, 0.1295f,
        "Hello, world!\n"
        "Ahoj, světe!\n"
        "Здравствуй, мир!\n"
        "Γεια σου κόσμε!\n"
        "Hej Världen!",
        _vertices, _indices, GL::BufferUsage::StaticDraw, Text::Alignment::MiddleCenter);

    GL::Renderer::enable(GL::Renderer::Feature::Blending);
    GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha);
    GL::Renderer::setBlendEquation(GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);

    _transformation = Matrix4::Matrix();
    _projection = Matrix4::perspectiveProjection(
        35.0_degf, Vector2{windowSize()}.aspectRatio(), 0.01f, 100.0f)*
        Matrix4::translation(Vector3::zAxis(-5.0f));

    obj = new Object3D{&_scene};
    Matrix4 scaling = Matrix4::scaling(Vector3{4.0});
    obj->transform(scaling);
    obj->translate(Vector3{1.0f, 0.0f, 0.0f});

    struct TriangleVertex {
        Vector3 position;
        Color3 color;
    };
    const TriangleVertex data[]{
        {{-0.5f, -0.2f, 0.0f}, 0xff0000_rgbf},    /* Left vertex, red color */
        {{ 0.5f, -0.7f, 0.0f}, 0x00ff00_rgbf},    /* Right vertex, green color */
        {{ 0.0f,  -0.7f, 0.0f}, 0x0000ff_rgbf}     /* Top vertex, blue color */
    };

    GL::Buffer buffer;
    buffer.setData(data);

    _mesh.setCount(3)
     .addVertexBuffer(std::move(buffer), 0,
    Shaders::VertexColor3D::Position{},
    Shaders::VertexColor3D::Color3{});

    //new ThingAble(_triang_shader, _mesh, *obj, _drawables);

    _dynamicText = new TextDrawable("Hello, SN", _font, &_cache, _shader, *obj, _drawables);

    GL::Renderer::setClearColor(0x002b36_rgbf);
}

void TextExample::viewportEvent(ViewportEvent& event) {
    GL::defaultFramebuffer.setViewport({{}, event.framebufferSize()});

    //_projection = Matrix3::scaling(Vector2::yScale(Vector2(event.windowSize()).aspectRatio()));
}

void TextExample::drawEvent() {

    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
    GL::Renderer::setClearColor(0x002b36_rgbf);

    _shader.bindVectorTexture(_cache.texture());

    if ( cnt % 60 == 0 ) {
        _dynamicText->updateText();
        redraw();
    }


    auto tp = _projection * _transformation;
    auto cam_tp = _camera->projectionMatrix() * _camera->cameraMatrix() * obj->transformationMatrix();

    /*
    _shader.setTransformationProjectionMatrix(tp)
        .setColor(Color3::fromHsv({216.0_degf, 0.85f, 1.0f}))
        .setOutlineColor(Color3{0.95f})
        .setOutlineRange(0.45f, 0.35f)
        .setSmoothness(0.025f/ _transformation.uniformScaling());
    _text.draw(_shader);

    _shader.setTransformationProjectionMatrix(tp)
        .setColor(Color3{1.0f})
        .setOutlineRange(0.5f, 1.0f)
        .setSmoothness(0.075f);
    //_text2->mesh().draw(_shader);
    */

    Utility::Debug{} << "tp\n" << tp << "\n";


    Utility::Debug{} << "cam_tp\n" << cam_tp << "\n";

    _camera->draw(_drawables);

    swapBuffers();
}

void TextExample::mouseScrollEvent(MouseScrollEvent& event) {
    if(!event.offset().y()) return;

    /*
    if(event.offset().y() > 0)
        _transformation = Matrix3::rotation(Deg(1.0f))*Matrix3::scaling(Vector2(1.1f))* _transformation;
    else
        _transformation = Matrix3::rotation(Deg(-1.0f))*Matrix3::scaling(Vector2(1.0f/1.1f))* _transformation;
    */

    event.setAccepted();
    redraw();
}


void TextExample::mouseMoveEvent(MouseMoveEvent& event) {
    if(!(event.buttons() & MouseMoveEvent::Button::Left)) return;

    const Vector2 delta = 3.0f*
        Vector2{event.position() - _previousMousePosition}/
        Vector2{GL::defaultFramebuffer.viewport().size()};

    _transformation =
        Matrix4::rotationX(Rad{delta.y()})*
        _transformation*
        Matrix4::rotationY(Rad{delta.x()});

    (*_cameraObject)
        .rotate(Rad{-delta.y()}, _cameraObject->transformation().right().normalized())
        .rotateY(Rad{-delta.x()});

    _previousMousePosition = event.position();

    event.setAccepted();

    redraw();
}



}}

MAGNUM_APPLICATION_MAIN(Magnum::Examples::TextExample)
