// SPDX-License-Identifier: GPL-3.0-or-later
#include "i18n.h"
#include "screencapture.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QImage>
#include <QTransform>
#include <QPointer>
#ifdef UMBRIEL_SCREEN
#include "ext-image-copy-capture-client.h"
#include "ext-image-capture-source-client.h"
#include <wayland-client.h>
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <climits>
#include <map>
namespace {
struct Output {wl_output *object=nullptr;uint32_t id=0;QString name,description;int width=0,height=0;};
struct CaptureBackend {
 wl_display *display=nullptr;wl_registry *registry=nullptr;wl_shm *shm=nullptr;ext_image_copy_capture_manager_v1 *manager=nullptr;ext_output_image_capture_source_manager_v1 *sources=nullptr;
 std::map<uint32_t,std::unique_ptr<Output>> outputs;ext_image_capture_source_v1 *source=nullptr;ext_image_copy_capture_session_v1 *session=nullptr;QString active;uint32_t width=0,height=0,format=UINT32_MAX,transform=0;bool constraints=false,stopped=false,gotFrame=false;
 ~CaptureBackend(){reset();for(auto &[id,out]:outputs)wl_output_destroy(out->object);if(sources)ext_output_image_capture_source_manager_v1_destroy(sources);if(manager)ext_image_copy_capture_manager_v1_destroy(manager);if(shm)wl_shm_destroy(shm);if(registry)wl_registry_destroy(registry);if(display)wl_display_disconnect(display);}
 void reset(){if(session)ext_image_copy_capture_session_v1_destroy(session);if(source)ext_image_capture_source_v1_destroy(source);session=nullptr;source=nullptr;active.clear();constraints=stopped=gotFrame=false;width=height=0;format=UINT32_MAX;if(display)wl_display_flush(display);}
 bool pump(const std::function<bool()> &done,int milliseconds=1800){QElapsedTimer clock;clock.start();while(!done()){
  if(wl_display_dispatch_pending(display)<0)return false;if(done())return true;
  while(wl_display_prepare_read(display)!=0){if(wl_display_dispatch_pending(display)<0)return false;if(done())return true;}
  const int flushed=wl_display_flush(display);if(flushed<0&&errno!=EAGAIN){wl_display_cancel_read(display);return false;}
  pollfd fd{wl_display_get_fd(display),short(POLLIN|(flushed<0?POLLOUT:0)),0};int ready=poll(&fd,1,qMax(0,milliseconds-int(clock.elapsed())));
  if(ready<0&&errno==EINTR){wl_display_cancel_read(display);continue;}if(ready<=0||fd.revents&(POLLHUP|POLLERR|POLLNVAL)){wl_display_cancel_read(display);return false;}
  if(fd.revents&POLLIN){if(wl_display_read_events(display)<0)return false;}else wl_display_cancel_read(display);
  if(clock.elapsed()>=milliseconds&&!done()){wl_display_dispatch_pending(display);return done();}
 }return true;}
 bool sync(){bool ready=false;auto *callback=wl_display_sync(display);static const wl_callback_listener listener{[](void *data,wl_callback *cb,uint32_t){*static_cast<bool*>(data)=true;wl_callback_destroy(cb);}};wl_callback_add_listener(callback,&listener,&ready);bool ok=pump([&]{return ready;});if(!ready)wl_callback_destroy(callback);return ok;}
 bool connect(){if(display)return sync();display=wl_display_connect(nullptr);if(!display)return false;registry=wl_display_get_registry(display);
  static const wl_output_listener outputListener{
   [](void*,wl_output*,int32_t,int32_t,int32_t,int32_t,int32_t,const char*,const char*,int32_t){},
   [](void *data,wl_output*,uint32_t flags,int32_t w,int32_t h,int32_t){if(flags&WL_OUTPUT_MODE_CURRENT){auto *out=static_cast<Output*>(data);out->width=w;out->height=h;}},
   [](void*,wl_output*){},[](void*,wl_output*,int32_t){},
   [](void *data,wl_output*,const char *name){static_cast<Output*>(data)->name=QString::fromUtf8(name).left(100);},
   [](void *data,wl_output*,const char *name){static_cast<Output*>(data)->description=QString::fromUtf8(name).left(160);}};
  static const wl_registry_listener listener{
   [](void *data,wl_registry *registry,uint32_t name,const char *interface,uint32_t version){auto *b=static_cast<CaptureBackend*>(data);const QByteArray type(interface);
    if(type==wl_shm_interface.name)b->shm=static_cast<wl_shm*>(wl_registry_bind(registry,name,&wl_shm_interface,1));
    else if(type==ext_image_copy_capture_manager_v1_interface.name)b->manager=static_cast<ext_image_copy_capture_manager_v1*>(wl_registry_bind(registry,name,&ext_image_copy_capture_manager_v1_interface,1));
    else if(type==ext_output_image_capture_source_manager_v1_interface.name)b->sources=static_cast<ext_output_image_capture_source_manager_v1*>(wl_registry_bind(registry,name,&ext_output_image_capture_source_manager_v1_interface,1));
    else if(type==wl_output_interface.name){auto out=std::make_unique<Output>();out->id=name;out->object=static_cast<wl_output*>(wl_registry_bind(registry,name,&wl_output_interface,qMin(version,4u)));wl_output_add_listener(out->object,&outputListener,out.get());b->outputs[name]=std::move(out);}},
   [](void *data,wl_registry*,uint32_t name){auto *b=static_cast<CaptureBackend*>(data);auto found=b->outputs.find(name);if(found!=b->outputs.end()){if(b->active.section(':',0,0)==QString::number(name))b->reset();wl_output_destroy(found->second->object);b->outputs.erase(found);}}};
  wl_registry_add_listener(registry,&listener,this);return sync()&&sync()&&shm&&manager&&sources;
 }
 QString sourceId(uint32_t id){const auto &out=outputs[id];return QString::number(id)+":"+QString::fromLatin1(QCryptographicHash::hash((out->name+"\n"+out->description).toUtf8(),QCryptographicHash::Sha256).toHex().left(16));}
 Reply list(){if(!connect()||!shm||!manager||!sources)return {false,I18n::text("El compositor no ofrece captura de pantalla compatible"),{}};QVariantList list;for(auto &[id,out]:outputs)list.append(QVariantMap{{"id",sourceId(id)},{"name",out->name.isEmpty()?QString(I18n::text("Pantalla %1")).arg(id):out->name},{"description",out->description},{"width",out->width},{"height",out->height}});return {true,{},list};}
 bool start(const QString &id){if(active==id&&session&&!stopped)return true;reset();bool valid=false;auto number=id.section(':',0,0).toUInt(&valid);if(!valid||!outputs.count(number)||sourceId(number)!=id)return false;source=ext_output_image_capture_source_manager_v1_create_source(sources,outputs[number]->object);session=ext_image_copy_capture_manager_v1_create_session(manager,source,EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);active=id;
  static const ext_image_copy_capture_session_v1_listener listener{
   [](void *data,ext_image_copy_capture_session_v1*,uint32_t w,uint32_t h){auto *b=static_cast<CaptureBackend*>(data);b->width=w;b->height=h;},
   [](void *data,ext_image_copy_capture_session_v1*,uint32_t f){if(f==WL_SHM_FORMAT_ARGB8888||f==WL_SHM_FORMAT_XRGB8888)static_cast<CaptureBackend*>(data)->format=f;},
   [](void*,ext_image_copy_capture_session_v1*,wl_array*){},[](void*,ext_image_copy_capture_session_v1*,uint32_t,wl_array*){},
   [](void *data,ext_image_copy_capture_session_v1*){static_cast<CaptureBackend*>(data)->constraints=true;},
   [](void *data,ext_image_copy_capture_session_v1*){static_cast<CaptureBackend*>(data)->stopped=true;}};
  ext_image_copy_capture_session_v1_add_listener(session,&listener,this);return pump([&]{return constraints||stopped;})&&constraints&&!stopped;
 }
 Reply frame(const QString &id){if(!connect()||!manager||!sources||!shm||!start(id))return {false,I18n::text("La pantalla ya no está disponible"),{}};if(!width||!height||width>8192||height>8192||uint64_t(width)*height*4>64*1024*1024||format==UINT32_MAX)return {false,I18n::text("La pantalla supera el formato o tamaño admitido"),{}};
  const uint32_t bufferWidth=width,bufferHeight=height,bufferFormat=format;
  const size_t size=size_t(bufferWidth)*bufferHeight*4;const int fd=memfd_create("umbriel-screen",MFD_CLOEXEC);if(fd<0)return {false,I18n::text("No se pudo preparar la captura"),{}};if(ftruncate(fd,off_t(size))<0){close(fd);return {false,I18n::text("No se pudo preparar la captura"),{}};}void *memory=mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(memory==MAP_FAILED){close(fd);return {false,I18n::text("No se pudo preparar la captura"),{}};}
  auto *pool=wl_shm_create_pool(shm,fd,int(size));auto *buffer=wl_shm_pool_create_buffer(pool,0,int(bufferWidth),int(bufferHeight),int(bufferWidth*4),bufferFormat);auto *frame=ext_image_copy_capture_session_v1_create_frame(session);
  struct Result{bool ready=false,failed=false;uint32_t transform=0;}result;
  static const ext_image_copy_capture_frame_v1_listener listener{
   [](void *data,ext_image_copy_capture_frame_v1*,uint32_t transform){static_cast<Result*>(data)->transform=transform;},
   [](void*,ext_image_copy_capture_frame_v1*,int32_t,int32_t,int32_t,int32_t){},[](void*,ext_image_copy_capture_frame_v1*,uint32_t,uint32_t,uint32_t){},
   [](void *data,ext_image_copy_capture_frame_v1*){static_cast<Result*>(data)->ready=true;},[](void *data,ext_image_copy_capture_frame_v1*,uint32_t){static_cast<Result*>(data)->failed=true;}};
  ext_image_copy_capture_frame_v1_add_listener(frame,&listener,&result);ext_image_copy_capture_frame_v1_attach_buffer(frame,buffer);ext_image_copy_capture_frame_v1_damage_buffer(frame,0,0,int(bufferWidth),int(bufferHeight));ext_image_copy_capture_frame_v1_capture(frame);bool delivered=pump([&]{return result.ready||result.failed||stopped;},1800);
  Reply reply{false,I18n::text("No se pudo recibir la imagen de la pantalla"),{}};
  if(delivered&&result.ready&&!result.failed&&!stopped){QImage borrowed(static_cast<uchar*>(memory),int(bufferWidth),int(bufferHeight),int(bufferWidth*4),bufferFormat==WL_SHM_FORMAT_XRGB8888?QImage::Format_RGB32:QImage::Format_ARGB32);QImage small=borrowed.scaled(1280,1280,Qt::KeepAspectRatio,Qt::SmoothTransformation);auto t=result.transform;if(t>=4)small=small.mirrored(true,false);if(t%4)small=small.transformed(QTransform().rotate(-90*int(t%4)));QByteArray jpeg;QBuffer out(&jpeg);out.open(QIODevice::WriteOnly);small.save(&out,"JPEG",65);if(jpeg.size()<=300000){reply={true,{},QVariantMap{{"jpeg",QString::fromLatin1(jpeg.toBase64())},{"width",small.width()},{"height",small.height()},{"source",id}}};gotFrame=true;}}
  else if(!delivered&&gotFrame&&wl_display_get_error(display)==0)reply={true,{},QVariantMap{{"unchanged",true}}};
  ext_image_copy_capture_frame_v1_destroy(frame);wl_buffer_destroy(buffer);wl_shm_pool_destroy(pool);munmap(memory,size);close(fd);wl_display_flush(display);if(result.failed||stopped)reset();return reply;
 }
};
}
#endif
struct ScreenCapture::Private {QThreadPool pool;
#ifdef UMBRIEL_SCREEN
 std::unique_ptr<CaptureBackend> backend;
#endif
 Private(){pool.setMaxThreadCount(1);pool.setExpiryTimeout(-1);}~Private(){pool.waitForDone();}
};
ScreenCapture::ScreenCapture(QObject *p):QObject(p),d(std::make_unique<Private>()){}
ScreenCapture::~ScreenCapture(){d->pool.waitForDone();}
bool ScreenCapture::available(){
#ifdef UMBRIEL_SCREEN
 return !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
#else
 return false;
#endif
}
void ScreenCapture::work(const QString &source,Done done){
#ifdef UMBRIEL_SCREEN
 if(m_pending){done({false,I18n::text("Hay una captura en curso"),{}});return;}m_pending=true;d->pool.start([this,source,done]{if(!d->backend)d->backend=std::make_unique<CaptureBackend>();auto reply=source.isEmpty()?d->backend->list():d->backend->frame(source);QMetaObject::invokeMethod(this,[this,done,reply]{m_pending=false;done(reply);},Qt::QueuedConnection);});
#else
 Q_UNUSED(source);done({false,I18n::text("Este receptor se compiló sin captura de pantalla"),{}});
#endif
}
void ScreenCapture::sources(Done done){work({},done);}void ScreenCapture::frame(const QString &source,Done done){work(source,done);}void ScreenCapture::release(){
#ifdef UMBRIEL_SCREEN
 d->pool.start([this]{d->backend.reset();});
#endif
}
