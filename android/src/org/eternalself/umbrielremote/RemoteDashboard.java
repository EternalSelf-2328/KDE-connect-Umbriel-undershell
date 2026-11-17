// SPDX-License-Identifier: GPL-3.0-or-later
package org.eternalself.umbrielremote;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Base64;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Authenticated controls: no Android accessibility or phone-screen permission. */
final class RemoteDashboard extends LinearLayout {
    final Activity activity;final UiDesign ui;final RemoteService owner;final Consumer<String> status;
    final LinearLayout body;final Map<String,JSONObject> actions=new LinkedHashMap<>();final ArrayList<Button> tabs=new ArrayList<>();
    JSONArray profiles=new JSONArray();JSONObject features=new JSONObject();int selected=5,generation=0,offset=0;boolean disposed=false,pointerEnabled=false,pointerStarting=false,moving=false;int pointerEpoch=0;
    boolean loaded=false,wantPointer=false,loadFailed=false;private HorizontalScrollView navBar;private Button pointerToggle;
    Touchpad touchpad;boolean favourites=false;String filter="",source="local",folder="",kind="";
    RemoteDashboard(Activity a,UiDesign design,RemoteService service,Consumer<String> message){
        super(a);activity=a;ui=design;owner=service;status=message;setOrientation(VERTICAL);source=a.getPreferences(0).getString("wallSource","local");folder=a.getPreferences(0).getString("wallFolder","");kind=a.getPreferences(0).getString("wallKind","");
        navBar=new HorizontalScrollView(a);navBar.setHorizontalScrollBarEnabled(false);
        LinearLayout row=new LinearLayout(a);row.setPadding(ui.dp(4),ui.dp(4),ui.dp(4),ui.dp(4));navBar.addView(row);
        String[] names={"Mouse","Navegación","Sonido","Apariencia","Fondos","Widgets"};int[] pages={1,0,2,6,3,7};
        for(int i=0;i<names.length;i++){final int index=pages[i];Button b=ui.button(names[i],false);b.setTag(index);b.setTextSize(13);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-2,-2);lp.setMarginEnd(i==names.length-1?0:ui.dp(UiDesign.COMPACT_GAP));row.addView(b,lp);tabs.add(b);b.setOnClickListener(v->{if(a instanceof MainActivity)((MainActivity)a).openControl(index);else select(index);});}
        ui.add(this,navBar,UiDesign.SECTION_GAP);body=ui.column();ui.add(this,body,0);load();
    }
    void showNavigation(boolean visible){navBar.setVisibility(visible?VISIBLE:GONE);}
    void activateMouse(){wantPointer=true;if(selected==1&&pointerToggle!=null&&!pointerEnabled&&!pointerStarting)pointerToggle.performClick();}
    void request(String type,JSONObject args,Consumer<JSONObject> complete){
        if(disposed||!owner.isAuthorized())return;
        owner.request(type,args,reply->activity.runOnUiThread(()->{if(!disposed&&owner.isAuthorized())complete.accept(reply);}));
    }
    JSONObject object(Object... values){JSONObject o=new JSONObject();try{for(int i=0;i<values.length;i+=2)o.put(values[i].toString(),values[i+1]);}catch(JSONException ignored){}return o;}
    void load(){
        loaded=false;loadFailed=false;status.accept("Cargando tus controles…");
        request("features",new JSONObject(),reply->{
            if(!reply.optBoolean("ok")){loadError();return;}features=reply.optJSONObject("value");if(features==null)features=new JSONObject();
            request("catalog",new JSONObject(),catalog->{if(!catalog.optBoolean("ok")){loadError();return;}JSONArray list=catalog.optJSONArray("value");if(list!=null)for(int i=0;i<list.length();i++){JSONObject action=list.optJSONObject(i);if(action!=null)actions.put(action.optString("id"),action);}
                request("profiles",new JSONObject(),saved->{JSONArray listProfiles=saved.optJSONArray("value");if(listProfiles!=null)profiles=listProfiles;loaded=true;boolean start=wantPointer;select(selected);if(start&&selected==1)activateMouse();status.accept(owner.automaticLocal()?"Todo listo · red local":"Todo listo · acceso remoto autorizado");});
            });
        });
    }
    private void loadError(){loadFailed=true;body.removeAllViews();LinearLayout c=card("No pude cargar los controles","La conexión sigue protegida. Vuelve a intentarlo.");Button retry=ui.button("Cargar de nuevo",true);retry.setOnClickListener(v->load());ui.add(c,retry,0);status.accept("No se pudieron cargar los controles");}
    void dispose(){if(disposed)return;stopPointer();disposed=true;generation++;}
    void stopPointer(){if(touchpad!=null)touchpad.setEnabled(false);if(pointerToggle!=null){pointerToggle.setText("Activar mouse");pointerToggle.setEnabled(true);}wantPointer=false;pointerEpoch++;owner.cancelMotion();if(touchpad!=null)touchpad.cancel();if(pointerEnabled||pointerStarting)request("pointer",object("op","stop"),r->{});pointerEnabled=false;pointerStarting=false;}
    void select(int page){stopPointer();selected=page;generation++;body.removeAllViews();pointerToggle=null;
        for(Button tab:tabs){boolean active=tab.getTag().equals(page);tab.setTextColor(active?ui.onAccent:ui.text);tab.setBackgroundTintList(android.content.res.ColorStateList.valueOf(active?ui.accent:ui.raised));tab.setSelected(active);}
        for(Button tab:tabs)if(tab.getTag().equals(page)&&navBar.getVisibility()==VISIBLE){navBar.getViewTreeObserver().addOnGlobalLayoutListener(new android.view.ViewTreeObserver.OnGlobalLayoutListener(){public void onGlobalLayout(){if(navBar.getViewTreeObserver().isAlive())navBar.getViewTreeObserver().removeOnGlobalLayoutListener(this);navBar.requestChildRectangleOnScreen((View)tab.getParent(),new android.graphics.Rect(tab.getLeft()-ui.dp(UiDesign.COMPACT_GAP),tab.getTop(),tab.getRight()+ui.dp(UiDesign.COMPACT_GAP),tab.getBottom()),true);}});break;}
        if(!loaded&&loadFailed){loadError();return;}
        if(!loaded){card("Preparando tus controles…","La primera carga tarda unos instantes.");return;}
        if(page==5)home();else if(page==0)navigation();else if(page==1)mouse();else if(page==2)sound();else if(page==3)wallpapers(0);else if(page==4)files();else if(page==6)appearance();else widgets();
    }
    private void home(){
        if(activity instanceof MainActivity)((MainActivity)activity).homeShortcuts(body,true);
        LinearLayout media=card("Reproducción","");
        row(media,transport("noctalia.media-previous","Pista anterior","previous",false),transport("noctalia.media-play-pause","Reproducir o pausar","play",true),transport("noctalia.media-next","Pista siguiente","next",false));
        TextView value=ui.label("Volumen · leyendo…",14,ui.muted,false);ui.add(media,value,0);SeekBar volume=new SeekBar(activity);volume.setMax(100);volume.setEnabled(false);ui.add(media,volume,0);Button mute=transport("noctalia.volume-mute","Silenciar","sound",false);
        readVolume(volume,value,mute);volume.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar s,int v,boolean user){if(user)value.setText("Volumen · "+v+" %");}public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){act("noctalia.volume-set",Integer.toString(s.getProgress()));}});

    }
    private Button transport(String id,String label,String icon,boolean primary){Button b=action(id,"",primary);b.setContentDescription(label);b.setCompoundDrawables(null,ui.icon(icon,primary?ui.onAccent:ui.text,24),null,null);b.setCompoundDrawablePadding(0);return b;}
    LinearLayout card(String title,String hint){LinearLayout c=ui.card();ui.add(c,ui.label(title,20,ui.text,true),8);if(!hint.isEmpty())ui.add(c,ui.label(hint,14,ui.muted,false),12);ui.add(body,c,16);return c;}
    Button action(String id,String title,boolean primary){Button b=ui.button(title,primary);b.setEnabled(actions.containsKey(id)&&actions.get(id).optBoolean("available"));b.setOnClickListener(v->act(id));return b;}
    void act(String id,String... args){
        Runnable run=()->request("action",object("action",id,"args",new JSONArray(Arrays.asList(args))),this::result);
        if(id.equals("noctalia.lock")||id.equals("umbriel.dpms-off"))new AlertDialog.Builder(activity).setTitle("Confirmar acción").setMessage("Esta acción puede interrumpir el acceso al escritorio.").setNegativeButton("Cancelar",null).setPositiveButton("Continuar",(d,w)->run.run()).show();else run.run();
    }
    void result(JSONObject reply){status.accept(reply.optBoolean("ok")?"Acción completada":reply.optString("error","No se pudo completar la acción"));}
    void row(LinearLayout parent,Button... buttons){LinearLayout row=new LinearLayout(activity);for(int i=0;i<buttons.length;i++){Button b=buttons[i];b.setTextSize(b.getText().length()==0?0:14);b.setPadding(ui.dp(10),ui.dp(12),ui.dp(10),ui.dp(12));LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-2,1);p.setMarginEnd(i==buttons.length-1?0:ui.dp(UiDesign.GAP));row.addView(b,p);}ui.add(parent,row,UiDesign.GAP);}
    void navigation(){
        LinearLayout nav=card("Muévete por Umbriel","Enfoca ventanas con las flechas y cambia de escritorio con los botones inferiores.");
        GridLayout pad=new GridLayout(activity);pad.setColumnCount(3);pad.setRowCount(3);
        String[][] keys={{"umbriel.focus-up","↑", "0","1"},{"umbriel.focus-left","←","1","0"},{"umbriel.overview-toggle","Vista general","1","1"},{"umbriel.focus-right","→","1","2"},{"umbriel.focus-down","↓","2","1"}};
        for(String[] k:keys){Button b=action(k[0],k[1],k[0].contains("overview"));b.setContentDescription(k[0].contains("focus")?"Enfocar ventana "+(k[0].endsWith("up")?"arriba":k[0].endsWith("down")?"abajo":k[0].endsWith("left")?"a la izquierda":"a la derecha"):"Abrir vista general");
            GridLayout.LayoutParams p=new GridLayout.LayoutParams(GridLayout.spec(Integer.parseInt(k[2])),GridLayout.spec(Integer.parseInt(k[3]),1f));p.width=0;p.height=ui.dp(68);p.setMargins(ui.dp(3),ui.dp(3),ui.dp(3),ui.dp(3));pad.addView(b,p);}
        ui.add(nav,pad,UiDesign.SECTION_GAP);
        row(nav,action("umbriel.workspace-previous","← Escritorio",false),action("umbriel.workspace-next","Escritorio →",false));
        row(nav,action("umbriel.workspace-toggle-show-desktop","Mostrar escritorio",false));
        Button advanced=ui.button("Pantallas y sesión…",false);ui.add(nav,advanced,8);LinearLayout extraNav=ui.column();extraNav.setVisibility(GONE);ui.add(nav,extraNav,0);advanced.setOnClickListener(v->extraNav.setVisibility(extraNav.getVisibility()==GONE?VISIBLE:GONE));
        row(extraNav,action("umbriel.output-focus-previous","Pantalla anterior",false),action("umbriel.output-focus-next","Pantalla siguiente",false));row(extraNav,action("umbriel.dpms-on","Encender pantallas",false),action("umbriel.dpms-off","Apagar pantallas…",false));row(extraNav,action("noctalia.lock","Bloquear sesión…",false));
    }
    void widgets(){
        LinearLayout widgets=card("Tus widgets","Controles de undershell y perfiles guardados.");
        row(widgets,action("undershell.edit-on","Editar",true),action("undershell.edit-off","Terminar",false));
        row(widgets,action("undershell.show","Mostrar widgets",false),action("undershell.hide","Ocultar widgets",false));
        Button more=ui.button("Más controles y perfiles…",false);ui.add(widgets,more,0);
        LinearLayout extra=ui.column();extra.setVisibility(GONE);ui.add(widgets,extra,0);more.setOnClickListener(v->extra.setVisibility(extra.getVisibility()==GONE?VISIBLE:GONE));
        for(JSONObject action:actions.values())if(action.optString("group").equals("undershell")&&!Arrays.asList("undershell.edit-on","undershell.edit-off","undershell.show","undershell.hide").contains(action.optString("id")))ui.add(extra,action(action.optString("id"),action.optString("name"),false),8);
        for(int i=0;i<profiles.length();i++){JSONObject profile=profiles.optJSONObject(i);if(profile==null)continue;Button b=ui.button(profile.optString("name","Perfil guardado"),false);b.setOnClickListener(v->new AlertDialog.Builder(activity).setTitle("Aplicar perfil").setMessage(profile.optString("name")).setNegativeButton("Cancelar",null).setPositiveButton("Aplicar",(d,w)->act("undershell.profile",profile.optString("id"))).show());ui.add(extra,b,8);}
    }
    void mouse(){
        LinearLayout c=card("Tu celular, un touchpad","Desliza para mover · toca para hacer clic · dos dedos para desplazar · mantén y arrastra.");
        if(!features.optBoolean("pointer")){ui.add(c,ui.label("El receptor necesita una compilación con Xwayland/XTest y una sesión gráfica disponible.",14,ui.muted,false),8);return;}
        Button enable=ui.button("Activar mouse",true);pointerToggle=enable;ui.add(c,enable,12);
        touchpad=new Touchpad(activity,ui,new Touchpad.Sink(){
            public boolean move(int dx,int dy){return pointerEnabled&&owner.pointerMove(dx,dy);}
            public void event(String op,String button,boolean down,int steps){if(pointerEnabled)request("pointer",object("op",op,"button",button,"down",down,"dy",steps),reply->{if(!reply.optBoolean("ok"))result(reply);});}
        });
        touchpad.setEnabled(false);LinearLayout.LayoutParams touchParams=new LinearLayout.LayoutParams(-1,ui.dp(240));touchParams.bottomMargin=ui.dp(UiDesign.GAP);c.addView(touchpad,touchParams);
        Button left=ui.button("Clic izquierdo",false),right=ui.button("Clic derecho",false);left.setEnabled(false);right.setEnabled(false);
        left.setOnClickListener(v->request("pointer",object("op","click","button","left"),this::result));right.setOnClickListener(v->request("pointer",object("op","click","button","right"),this::result));row(c,left,right);
        enable.setOnClickListener(v->{if(pointerEnabled){stopPointer();touchpad.setEnabled(false);left.setEnabled(false);right.setEnabled(false);enable.setText("Activar mouse");status.accept("Mouse detenido");return;}
            enable.setEnabled(false);pointerStarting=true;final int epoch=++pointerEpoch;request("pointer",object("op","start"),reply->{if(epoch!=pointerEpoch)return;pointerStarting=false;enable.setEnabled(true);pointerEnabled=reply.optBoolean("ok");touchpad.setEnabled(pointerEnabled);left.setEnabled(pointerEnabled);right.setEnabled(pointerEnabled);enable.setText(pointerEnabled?"Detener mouse":"Activar mouse");status.accept(pointerEnabled?"Mouse activado. Si Umbriel pide permiso, apruébalo localmente en la computadora.":reply.optString("error"));});});
        ui.add(c,ui.label("Sensibilidad",14,ui.muted,false),4);SeekBar speed=new SeekBar(activity);speed.setMax(350);touchpad.sensitivity=activity.getPreferences(0).getFloat("mouseSensitivity",2.4f);speed.setProgress(Math.round((touchpad.sensitivity-.5f)*100));speed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar s,int p,boolean user){touchpad.sensitivity=.5f+p/100f;}public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){activity.getPreferences(0).edit().putFloat("mouseSensitivity",touchpad.sensitivity).apply();}});ui.add(c,speed,4);
    }
    void sound(){
        LinearLayout c=card("Sonido","Ajusta el volumen al soltar el control.");TextView value=ui.label("Volumen · leyendo…",16,ui.text,true);ui.add(c,value,4);
        SeekBar volume=new SeekBar(activity);volume.setMax(100);volume.setEnabled(false);ui.add(c,volume,8);
        Button mute=action("noctalia.volume-mute","Silenciar",false);
        readVolume(volume,value,mute);
        mute.setOnClickListener(v->request("action",object("action","noctalia.volume-mute","args",new JSONArray()),reply->{result(reply);readVolume(volume,value,mute);}));
        volume.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar s,int p,boolean user){if(user)value.setText("Volumen · "+p+" %");}public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){act("noctalia.volume-set",Integer.toString(s.getProgress()));}});
        row(c,action("noctalia.volume-down","− Volumen",false),mute,action("noctalia.volume-up","+ Volumen",false));
        row(c,action("noctalia.media-previous","Anterior",false),action("noctalia.media-play-pause","Reproducir",true),action("noctalia.media-next","Siguiente",false));
    }
    void appearance(){
        LinearLayout look=card("Apariencia","Elige el tema que deseas aplicar.");row(look,action("noctalia.theme-light","Claro",false),action("noctalia.theme-dark","Oscuro",false));
        LinearLayout quiet=card("Concentración y reposo","");row(quiet,action("noctalia.dnd-on","No molestar",false),action("noctalia.dnd-off","Permitir avisos",false));row(quiet,action("noctalia.caffeine-enable","Evitar reposo",false),action("noctalia.caffeine-disable","Permitir reposo",false));
    }
    void files(){
        LinearLayout c=card("Tus archivos","Envía directamente por la red local o Tailscale, con conexión cifrada. Las descargas se guardan en Descargas/Umbriel Connect.");
        if(!features.optBoolean("files")){ui.add(c,ui.label("Actualiza el receptor de la computadora para transferir archivos.",14,ui.muted,false),8);return;}
        Button choose=ui.button("Elegir archivo…",true);choose.setOnClickListener(v->{if(activity instanceof MainActivity)((MainActivity)activity).chooseFile();});ui.add(c,choose,8);
        if(activity instanceof MainActivity&&((MainActivity)activity).pendingFile!=null){Button send=ui.button("Enviar archivo seleccionado",false);send.setOnClickListener(v->owner.files.upload(((MainActivity)activity).pendingFile));ui.add(c,send,8);}
        ui.add(c,ui.label("El selector de Android conserva una conexión válida. En la red local, los archivos se reciben automáticamente sin abrirse; fuera de ella se pide aceptar cada descarga.",14,ui.muted,false),8);
    }
    void readVolume(SeekBar volume,TextView value,Button mute){request("audio-state",new JSONObject(),reply->{JSONObject state=reply.optJSONObject("value");if(reply.optBoolean("ok")&&state!=null){volume.setProgress(state.optInt("volume"));value.setText("Volumen · "+state.optInt("volume")+" %"+(state.optBoolean("muted")?" · silenciado":""));mute.setContentDescription(state.optBoolean("muted")?"Activar audio":"Silenciar");if(mute.getText().length()>0)mute.setText(state.optBoolean("muted")?"Activar audio":"Silenciar");volume.setEnabled(true);}else value.setText("Volumen actual no disponible");});}
    void wallpapers(int next){
        offset=next;generation++;final int version=generation;body.removeAllViews();LinearLayout c=card("Tus fondos","Biblioteca de undershell-wall · miniaturas ligeras, sin reproducir animaciones.");
        if(!features.optBoolean("wallpapers")){ui.add(c,ui.label("Instala o inicia undershell-wall para mostrar su biblioteca.",14,ui.muted,false),8);return;}
        EditText query=new EditText(activity);query.setSingleLine(true);query.setHint("Buscar fondos");query.setText(filter);ui.add(c,query,UiDesign.GAP);
        Button find=ui.button("Buscar",false),fav=ui.button(favourites?"Ver todos":"Solo favoritos",false);row(c,find,fav);
        find.setOnClickListener(v->{filter=query.getText().toString().trim();wallpapers(0);});fav.setOnClickListener(v->{favourites=!favourites;filter=query.getText().toString().trim();wallpapers(0);});
        Spinner origins=new Spinner(activity);String[] originIds={"local","engine",""};origins.setAdapter(new ArrayAdapter<String>(activity,android.R.layout.simple_spinner_dropdown_item,new String[]{"Mis carpetas","Wallpaper Engine","Todos los orígenes"}));
        int originIndex=Arrays.asList(originIds).indexOf(source);origins.setSelection(Math.max(0,originIndex));ui.add(c,origins,UiDesign.GAP);
        Spinner kinds=new Spinner(activity);String[] kindIds={"","static","video","we"};kinds.setAdapter(new ArrayAdapter<String>(activity,android.R.layout.simple_spinner_dropdown_item,new String[]{"Todos los tipos","Imágenes","Videos","Escenas WE"}));kinds.setSelection(Math.max(0,Arrays.asList(kindIds).indexOf(kind)));ui.add(c,kinds,UiDesign.GAP);
        Spinner folders=new Spinner(activity);ui.add(c,folders,UiDesign.GAP);
        origins.setOnItemSelectedListener(selection(index->{if(!source.equals(originIds[index])){source=originIds[index];folder="";saveFilters();wallpapers(0);}}));
        kinds.setOnItemSelectedListener(selection(index->{if(!kind.equals(kindIds[index])){kind=kindIds[index];saveFilters();wallpapers(0);}}));
        TextView count=ui.label("Cargando biblioteca…",14,ui.muted,false);ui.add(c,count,8);
        GridLayout grid=new GridLayout(activity);final int columns=activity.getResources().getConfiguration().screenWidthDp>=600?3:2;grid.setColumnCount(columns);ui.add(c,grid,8);
        request("wallpapers",object("offset",offset,"filter",filter,"favourites",favourites,"source",source,"folder",folder,"kind",kind),reply->{
            if(version!=generation)return;JSONObject page=reply.optJSONObject("value");if(!reply.optBoolean("ok")||page==null){count.setText(reply.optString("error","No se pudo cargar la biblioteca"));return;}
            JSONArray groups=page.optJSONArray("folders");ArrayList<String> labels=new ArrayList<>(),ids=new ArrayList<>();labels.add("Todas las carpetas");ids.add("");if(groups!=null)for(int i=0;i<groups.length();i++){JSONObject group=groups.optJSONObject(i);if(group!=null){labels.add(group.optString("name")+" ("+group.optInt("count")+")");ids.add(group.optString("id"));}}
            folders.setAdapter(new ArrayAdapter<String>(activity,android.R.layout.simple_spinner_dropdown_item,labels));folders.setSelection(Math.max(0,ids.indexOf(folder)));folders.setOnItemSelectedListener(selection(index->{if(!folder.equals(ids.get(index))){folder=ids.get(index);saveFilters();wallpapers(0);}}));
            JSONArray items=page.optJSONArray("items");if(items==null)return;count.setText(page.optInt("total")==0?"No hay fondos que coincidan.":(offset+1)+"–"+(offset+items.length())+" de "+page.optInt("total"));
            ArrayList<ImageView> images=new ArrayList<>();
            for(int i=0;i<items.length();i++){JSONObject item=items.optJSONObject(i);if(item==null)continue;LinearLayout tile=ui.column();tile.setPadding(ui.dp(UiDesign.COMPACT_GAP),ui.dp(UiDesign.COMPACT_GAP),ui.dp(UiDesign.COMPACT_GAP),ui.dp(UiDesign.COMPACT_GAP));tile.setBackground(ui.shape(ui.raised,18));
                ImageView image=new ImageView(activity);image.setBackground(ui.shape(ui.background,14));image.setClipToOutline(true);image.setScaleType(ImageView.ScaleType.CENTER_CROP);image.setContentDescription("Vista previa de "+item.optString("name"));tile.addView(image,new LinearLayout.LayoutParams(-1,ui.dp(106)));images.add(image);
                Space imageGap=new Space(activity);tile.addView(imageGap,new LinearLayout.LayoutParams(1,ui.dp(UiDesign.COMPACT_GAP)));TextView name=ui.label(item.optString("name"),13,ui.text,true);name.setMaxLines(2);name.setEllipsize(android.text.TextUtils.TruncateAt.END);ui.add(tile,name,3);ui.add(tile,ui.label(item.optString("kind").equals("static")?"Imagen":"Animado",12,ui.muted,false),0);
                tile.setFocusable(true);tile.setContentDescription("Seleccionar fondo "+item.optString("name"));tile.setOnClickListener(v->new AlertDialog.Builder(activity).setTitle("Aplicar fondo").setView(preview(image)).setMessage(item.optString("name")+"\nSe aplicará en todas las pantallas."+(item.optString("kind").equals("static")?"":"\nUn fondo animado puede aumentar el consumo de energía.")).setNegativeButton("Cancelar",null).setPositiveButton("Aplicar",(d,w)->request("wallpaper-apply",object("id",item.optString("id")),this::result)).show());
                GridLayout.LayoutParams cell=new GridLayout.LayoutParams(GridLayout.spec(i/columns),GridLayout.spec(i%columns,1f));cell.width=0;cell.setMargins(0,0,i%columns==columns-1?0:ui.dp(UiDesign.GAP),ui.dp(UiDesign.GAP));grid.addView(tile,cell);
            }
            Button previous=ui.button("← Anterior",false),more=ui.button("Siguiente →",false);previous.setEnabled(offset>0);more.setEnabled(page.optBoolean("hasMore"));previous.setOnClickListener(v->wallpapers(Math.max(0,offset-12)));more.setOnClickListener(v->wallpapers(offset+12));row(c,previous,more);
            thumbnails(items,images,0,version);
        });
    }
    AdapterView.OnItemSelectedListener selection(java.util.function.IntConsumer change){final int epoch=generation;return new AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?> parent,View view,int position,long id){if(!disposed&&selected==3&&epoch==generation)change.accept(position);}public void onNothingSelected(AdapterView<?> parent){}};}
    void saveFilters(){activity.getPreferences(0).edit().putString("wallSource",source).putString("wallFolder",folder).putString("wallKind",kind).apply();}
    View preview(ImageView source){ImageView large=new ImageView(activity);large.setAdjustViewBounds(true);large.setScaleType(ImageView.ScaleType.FIT_CENTER);large.setMaxHeight(ui.dp(220));large.setMinimumHeight(ui.dp(160));if(source.getDrawable() instanceof android.graphics.drawable.BitmapDrawable)large.setImageBitmap(((android.graphics.drawable.BitmapDrawable)source.getDrawable()).getBitmap());return large;}
    void thumbnails(JSONArray items,ArrayList<ImageView> images,int index,int version){
        if(disposed||version!=generation||index>=items.length()||index>=images.size())return;
        JSONObject item=items.optJSONObject(index);if(item==null||!item.optBoolean("hasPreview")){thumbnails(items,images,index+1,version);return;}
        request("wallpaper-thumb",object("id",item.optString("id")),reply->{if(version!=generation)return;JSONObject value=reply.optJSONObject("value");
            if(reply.optBoolean("ok")&&value!=null)try{String encoded=value.optString("jpeg");if(encoded.length()<=140000){byte[] bytes=Base64.decode(encoded,Base64.NO_WRAP);BitmapFactory.Options bounds=new BitmapFactory.Options();bounds.inJustDecodeBounds=true;BitmapFactory.decodeByteArray(bytes,0,bytes.length,bounds);if(bounds.outWidth>0&&bounds.outWidth<=640&&bounds.outHeight>0&&bounds.outHeight<=400){Bitmap bitmap=BitmapFactory.decodeByteArray(bytes,0,bytes.length);if(bitmap!=null)images.get(index).setImageBitmap(bitmap);}}}catch(IllegalArgumentException ignored){}
            thumbnails(items,images,index+1,version);
        });
    }
}
