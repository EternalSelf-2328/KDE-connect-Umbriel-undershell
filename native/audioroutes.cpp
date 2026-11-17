// SPDX-License-Identifier: GPL-3.0-or-later
#include "audioroutes.h"
#include <QPointer>
#include <QJsonArray>
#ifdef UMBRIEL_PULSE
#include <pulse/pulseaudio.h>
#include <vector>
struct AudioRoutes::Private {
    struct Job {Private *d;quint64 id;Done done;bool finished=false;QString defaultName,targetName;uint32_t target=PA_INVALID_INDEX;QVariantList outputs;int remaining=0,moved=0,failed=0;std::vector<pa_operation*> ops;};
    AudioRoutes *q;pa_threaded_mainloop *loop=nullptr;pa_context *context=nullptr;QHash<quint64,std::shared_ptr<Job>> jobs;quint64 serial=0;
    explicit Private(AudioRoutes *owner):q(owner){loop=pa_threaded_mainloop_new();if(!loop)return;if(pa_threaded_mainloop_start(loop)<0){pa_threaded_mainloop_free(loop);loop=nullptr;return;}connect();}
    ~Private(){if(!loop)return;pa_threaded_mainloop_lock(loop);for(auto job:jobs)for(auto *op:job->ops){pa_operation_cancel(op);pa_operation_unref(op);}jobs.clear();if(context){pa_context_set_state_callback(context,nullptr,nullptr);pa_context_set_subscribe_callback(context,nullptr,nullptr);pa_context_disconnect(context);pa_context_unref(context);}pa_threaded_mainloop_unlock(loop);pa_threaded_mainloop_stop(loop);pa_threaded_mainloop_free(loop);}
    void connect(){if(!loop)return;pa_threaded_mainloop_lock(loop);if(context){pa_context_set_state_callback(context,nullptr,nullptr);pa_context_disconnect(context);pa_context_unref(context);}context=pa_context_new(pa_threaded_mainloop_get_api(loop),"Umbriel Connect");
        if(!context){pa_threaded_mainloop_unlock(loop);return;}pa_context_set_state_callback(context,[](pa_context *c,void *data){auto *d=static_cast<Private*>(data);auto state=pa_context_get_state(c);if(state==PA_CONTEXT_READY){pa_context_set_subscribe_callback(c,[](pa_context*,pa_subscription_event_type_t,uint32_t,void *data){auto *d=static_cast<Private*>(data);QMetaObject::invokeMethod(d->q,[q=d->q]{emit q->changed();},Qt::QueuedConnection);},d);if(auto *op=pa_context_subscribe(c,pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SERVER|PA_SUBSCRIPTION_MASK_SINK),nullptr,nullptr))pa_operation_unref(op);}
            if(state==PA_CONTEXT_READY||state==PA_CONTEXT_FAILED||state==PA_CONTEXT_TERMINATED)QMetaObject::invokeMethod(d->q,[q=d->q]{emit q->changed();},Qt::QueuedConnection);
        },this);pa_context_connect(context,nullptr,PA_CONTEXT_NOAUTOSPAWN,nullptr);pa_threaded_mainloop_unlock(loop);
    }
    bool ready(){if(!loop)return false;pa_threaded_mainloop_lock(loop);auto state=context?pa_context_get_state(context):PA_CONTEXT_UNCONNECTED;pa_threaded_mainloop_unlock(loop);if((state==PA_CONTEXT_FAILED||state==PA_CONTEXT_TERMINATED)&&jobs.isEmpty())connect();return state==PA_CONTEXT_READY;}
    void finish(quint64 id,Reply result){auto found=jobs.find(id);if(found==jobs.end())return;auto job=found.value();pa_threaded_mainloop_lock(loop);job->finished=true;for(auto *op:job->ops){pa_operation_cancel(op);pa_operation_unref(op);}pa_threaded_mainloop_unlock(loop);jobs.erase(found);job->done(result);}
    static void complete(Job *job,Reply r){if(job->finished)return;job->finished=true;const auto id=job->id;auto *d=job->d;QMetaObject::invokeMethod(d->q,[d,id,r]{d->finish(id,r);},Qt::QueuedConnection);}
    static bool operation(Job *job,pa_operation *op){if(op){job->ops.push_back(op);return true;}complete(job,{false,"El servidor de audio rechazó la operación",{}});return false;}
    std::shared_ptr<Job> create(Done done){if(!ready()){done({false,"Audio no disponible todavía. Vuelve a intentarlo",{}});return {};}if(jobs.size()>=8){done({false,"Hay una consulta de audio en curso",{}});return {};}
        auto job=std::make_shared<Job>();job->d=this;job->id=++serial;job->done=done;jobs[job->id]=job;QTimer::singleShot(5000,q,[this,id=job->id]{finish(id,{false,"El servidor de audio no respondió",{}});});return job;}
    static void sinks(pa_context*,const pa_sink_info *info,int end,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(end){if(end<0){complete(job,{false,"No se pudieron leer las salidas de audio",{}});return;}complete(job,{true,{},QVariantMap{{"outputs",job->outputs}}});return;}
        if(info&&info->name&&job->outputs.size()<128){QString description=QString::fromUtf8(info->description?info->description:info->name).left(256);job->outputs.append(QVariantMap{{"id",QString::number(info->index)},{"name",description},{"default",job->defaultName==QString::fromUtf8(info->name)},{"volume",qBound(0,qRound(double(pa_cvolume_avg(&info->volume))/PA_VOLUME_NORM*100),100)},{"muted",bool(info->mute)}});}}
    static void server(pa_context *c,const pa_server_info *info,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(!info){complete(job,{false,"No se pudo consultar la salida predeterminada",{}});return;}job->defaultName=QString::fromUtf8(info->default_sink_name?info->default_sink_name:"");operation(job,pa_context_get_sink_info_list(c,sinks,job));}
    static void moved(pa_context*,int success,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(success)++job->moved;else ++job->failed;if(--job->remaining==0)complete(job,{true,{},QVariantMap{{"moved",job->moved},{"failed",job->failed}}});}
    static void inputs(pa_context *c,const pa_sink_input_info *info,int end,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(end){if(end<0)++job->failed;if(--job->remaining==0)complete(job,{true,{},QVariantMap{{"moved",job->moved},{"failed",job->failed}}});return;}
        if(info&&info->sink!=job->target){++job->remaining;auto *op=pa_context_move_sink_input_by_index(c,info->index,job->target,moved,job);if(op)job->ops.push_back(op);else{++job->failed;--job->remaining;}}}
    static void selected(pa_context *c,int success,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(!success){complete(job,{false,"No se pudo cambiar la salida predeterminada",{}});return;}job->remaining=1;operation(job,pa_context_get_sink_input_info_list(c,inputs,job));}
    static void target(pa_context *c,const pa_sink_info *info,int end,void *data){auto *job=static_cast<Job*>(data);if(job->finished)return;if(end){if(end<0||job->targetName.isEmpty()){complete(job,{false,"La salida ya no está disponible",{}});return;}operation(job,pa_context_set_default_sink(c,job->targetName.toUtf8().constData(),selected,job));return;}if(info&&info->name)job->targetName=QString::fromUtf8(info->name);}
};
#else
struct AudioRoutes::Private {};
#endif
AudioRoutes::AudioRoutes(QObject *p):QObject(p){
#ifdef UMBRIEL_PULSE
    d=std::make_unique<Private>(this);
#endif
}
AudioRoutes::~AudioRoutes()=default;
bool AudioRoutes::available(){
#ifdef UMBRIEL_PULSE
return true;
#else
return false;
#endif
}
void AudioRoutes::outputs(Done done){
#ifdef UMBRIEL_PULSE
    auto job=d->create(done);if(!job)return;pa_threaded_mainloop_lock(d->loop);Private::operation(job.get(),pa_context_get_server_info(d->context,Private::server,job.get()));pa_threaded_mainloop_unlock(d->loop);
#else
done({false,"El receptor se compiló sin soporte de salidas de audio",{}});
#endif
}
void AudioRoutes::select(const QString &id,Done done){
#ifdef UMBRIEL_PULSE
    bool valid=false;auto index=id.toUInt(&valid);if(!valid||QString::number(index)!=id||index==PA_INVALID_INDEX){done({false,"Salida de audio inválida",{}});return;}
    auto job=d->create(done);if(!job)return;job->target=index;pa_threaded_mainloop_lock(d->loop);Private::operation(job.get(),pa_context_get_sink_info_by_index(d->context,index,Private::target,job.get()));pa_threaded_mainloop_unlock(d->loop);
#else
Q_UNUSED(id);done({false,"El receptor se compiló sin soporte de salidas de audio",{}});
#endif
}
