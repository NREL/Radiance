/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm.c
 */
#include "standard.h"
#include "sm_flag.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm_qtree.h"
#include "sm_stree.h"
#include "sm.h"


SM *smMesh = NULL;
double smDist_sum=0;
#define S_INC 1000
int smNew_tri_cnt=0;
int smNew_tri_size=0;

#define SM_MIN_SAMP_DIFF 1e-4/* min edge length in radians */

/* Each edge extends .5536 radians - 31.72 degrees */
static FVECT icosa_verts[162] = 
{{-0.018096,0.495400,0.868477},{0.018614,-0.554780,0.831789},
{0.850436,0.011329,0.525956},{0.850116,0.048016,-0.524402},
{-0.850116,-0.048016,0.524402},{-0.850436,-0.011329,-0.525956},
{0.495955,0.867825,0.030147},{-0.555329,0.831118,0.029186},
{0.555329,-0.831118,-0.029186},{-0.495955,-0.867825,-0.030147},
{-0.018614,0.554780,-0.831789},{0.018096,-0.495400,-0.868477},
{0.000305,-0.034906,0.999391},{0.489330,0.297837,0.819664},
{0.510900,-0.319418,0.798094},{-0.488835,-0.354308,0.797186},
{-0.510409,0.262947,0.818744},{0.999391,0.034875,0.000914},
{0.791335,0.516672,0.326862},{0.791142,0.538234,-0.290513},
{0.826031,-0.460219,-0.325378},{0.826216,-0.481780,0.291985},
{-0.999391,-0.034875,-0.000914},{-0.826216,0.481780,-0.291985},
{-0.826031,0.460219,0.325378},{-0.791142,-0.538234,0.290513},
{-0.791335,-0.516672,-0.326862},{-0.034911,0.998782,0.034881},
{0.280899,0.801316,0.528194},{-0.337075,0.779739,0.527624},
{-0.337377,0.814637,-0.471746},{0.280592,0.836213,-0.471186},
{0.034911,-0.998782,-0.034881},{-0.280592,-0.836213,0.471186},
{0.337377,-0.814637,0.471746},{0.337075,-0.779739,-0.527624},
{-0.280899,-0.801316,-0.528194},{-0.000305,0.034906,-0.999391},
{0.510409,-0.262947,-0.818744},{0.488835,0.354308,-0.797186},
{-0.510900,0.319418,-0.798094},{-0.489330,-0.297837,-0.819664},
{0.009834,-0.306509,0.951817},{0.268764,-0.186284,0.945021},
{0.275239,-0.454405,0.847207},{-0.009247,0.239357,0.970888},
{0.244947,0.412323,0.877491},{0.257423,0.138235,0.956360},
{0.525850,-0.011345,0.850502},{0.696394,0.160701,0.699436},
{0.707604,-0.160141,0.688223},{-0.268186,0.119892,0.955878},
{-0.274715,0.394186,0.877011},{-0.244420,-0.472542,0.846737},
{-0.256843,-0.204627,0.944542},{-0.525332,-0.048031,0.849541},
{-0.695970,-0.209123,0.686945},{-0.707182,0.111718,0.698149},
{0.961307,0.043084,-0.272090},{0.941300,0.301289,-0.152245},
{0.853076,0.304715,-0.423568},{0.961473,0.024015,0.273848},
{0.853344,0.274439,0.443269},{0.941400,0.289953,0.172315},
{0.831918,0.554570,0.019109},{0.669103,0.719629,0.185565},
{0.669003,0.730836,-0.135332},{0.959736,-0.234941,0.153979},
{0.871472,-0.244526,0.425140},{0.871210,-0.214250,-0.441690},
{0.959638,-0.223606,-0.170573},{0.868594,-0.495213,-0.017555},
{0.717997,-0.671205,-0.184294},{0.718092,-0.682411,0.136596},
{-0.961307,-0.043084,0.272090},{-0.959638,0.223606,0.170573},
{-0.871210,0.214250,0.441690},{-0.961473,-0.024015,-0.273848},
{-0.871472,0.244526,-0.425140},{-0.959736,0.234941,-0.153979},
{-0.868594,0.495213,0.017555},{-0.718092,0.682411,-0.136596},
{-0.717997,0.671205,0.184294},{-0.941400,-0.289953,-0.172315},
{-0.853344,-0.274439,-0.443269},{-0.853076,-0.304715,0.423568},
{-0.941300,-0.301289,0.152245},{-0.831918,-0.554570,-0.019109},
{-0.669003,-0.730836,0.135332},{-0.669103,-0.719629,-0.185565},
{-0.306809,0.951188,0.033302},{-0.195568,0.935038,0.295731},
{-0.463858,0.837300,0.289421},{0.239653,0.970270,0.033802},
{0.403798,0.867595,0.290218},{0.129326,0.946383,0.296031},
{-0.029535,0.831255,0.555106},{0.136603,0.674019,0.725974},
{-0.184614,0.662804,0.725678},{0.129164,0.964728,-0.229382},
{0.403637,0.885733,-0.229245},{-0.464015,0.855438,-0.230036},
{-0.195726,0.953383,-0.229677},{-0.029855,0.867949,-0.495755},
{-0.185040,0.711807,-0.677562},{0.136173,0.723022,-0.677271},
{0.306809,-0.951188,-0.033302},{0.195726,-0.953383,0.229677},
{0.464015,-0.855438,0.230036},{-0.239653,-0.970270,-0.033802},
{-0.403637,-0.885733,0.229245},{-0.129164,-0.964728,0.229382},
{0.029855,-0.867949,0.495755},{-0.136173,-0.723022,0.677271},
{0.185040,-0.711807,0.677562},{-0.129326,-0.946383,-0.296031},
{-0.403798,-0.867595,-0.290218},{0.463858,-0.837300,-0.289421}
,{0.195568,-0.935038,-0.295731},{0.029535,-0.831255,-0.555106},
{0.184614,-0.662804,-0.725678},{-0.136603,-0.674019,-0.725974},
{-0.009834,0.306509,-0.951817},{0.256843,0.204627,-0.944542},
{0.244420,0.472542,-0.846737},{0.009247,-0.239357,-0.970888},
{0.274715,-0.394186,-0.877011},{0.268186,-0.119892,-0.955878},
{0.525332,0.048031,-0.849541},{0.707182,-0.111718,-0.698149},
{0.695970,0.209123,-0.686945},{-0.257423,-0.138235,-0.956360},
{-0.244947,-0.412323,-0.877491},{-0.275239,0.454405,-0.847207},
{-0.268764,0.186284,-0.945021},{-0.525850,0.011345,-0.850502},
{-0.707604,0.160141,-0.688223},{-0.696394,-0.160701,-0.699436},
{0.563717,0.692920,0.449538},{0.673284,0.428212,0.602763},
{0.404929,0.577853,0.708603},{0.445959,-0.596198,0.667584},
{0.702960,-0.421213,0.573086},{0.611746,-0.681577,0.401523},
{-0.445543,0.548166,0.707818},{-0.702606,0.380190,0.601498},
{-0.611490,0.651895,0.448456},{-0.563454,-0.722602,0.400456},
{-0.672921,-0.469235,0.571835},{-0.404507,-0.625886,0.666814},
{0.404507,0.625886,-0.666814},{0.672921,0.469235,-0.571835},
{0.563454,0.722602,-0.400456},{0.611490,-0.651895,-0.448456},
{0.702606,-0.380190,-0.601498},{0.445543,-0.548166,-0.707818},
{-0.611746,0.681577,-0.401523},{-0.702960,0.421213,-0.573086},
{-0.445959,0.596198,-0.667584},{-0.404929,-0.577853,-0.708603},
{-0.673284,-0.428212,-0.602763},{-0.563717,-0.692920,-0.449538}};
static int icosa_tris[320][3] =
{ {1,42,44},{42,12,43},{44,43,14},{42,43,44},{12,45,47},{45,0,46},{47,46,13},
{45,46,47},{14,48,50},{48,13,49},{50,49,2},{48,49,50},{12,47,43},{47,13,48},
{43,48,14},{47,48,43},{0,45,52},{45,12,51},{52,51,16},{45,51,52},{12,42,54},
{42,1,53},{54,53,15},{42,53,54},{16,55,57},{55,15,56},{57,56,4},{55,56,57},
{12,54,51},{54,15,55},{51,55,16},{54,55,51},{3,58,60},{58,17,59},{60,59,19},
{58,59,60},{17,61,63},{61,2,62},{63,62,18},{61,62,63},{19,64,66},{64,18,65},
{66,65,6},{64,65,66},{17,63,59},{63,18,64},{59,64,19},{63,64,59},{2,61,68},
{61,17,67},{68,67,21},{61,67,68},{17,58,70},{58,3,69},{70,69,20},{58,69,70},
{21,71,73},{71,20,72},{73,72,8},{71,72,73},{17,70,67},{70,20,71},{67,71,21},
{70,71,67},{4,74,76},{74,22,75},{76,75,24},{74,75,76},{22,77,79},{77,5,78},
{79,78,23},{77,78,79},{24,80,82},{80,23,81},{82,81,7},{80,81,82},{22,79,75},
{79,23,80},{75,80,24},{79,80,75},{5,77,84},{77,22,83},{84,83,26},{77,83,84},
{22,74,86},{74,4,85},{86,85,25},{74,85,86},{26,87,89},{87,25,88},{89,88,9},
{87,88,89},{22,86,83},{86,25,87},{83,87,26},{86,87,83},{7,90,92},{90,27,91},
{92,91,29},{90,91,92},{27,93,95},{93,6,94},{95,94,28},{93,94,95},{29,96,98},
{96,28,97},{98,97,0},{96,97,98},{27,95,91},{95,28,96},{91,96,29},{95,96,91},
{6,93,100},{93,27,99},{100,99,31},{93,99,100},{27,90,102},{90,7,101},
{102,101,30},{90,101,102},{31,103,105},{103,30,104},{105,104,10},{103,104,105},
{27,102,99},{102,30,103},{99,103,31},{102,103,99},{8,106,108},{106,32,107},
{108,107,34},{106,107,108},{32,109,111},{109,9,110},{111,110,33},{109,110,111},
{34,112,114},{112,33,113},{114,113,1},{112,113,114},{32,111,107},{111,33,112},
{107,112,34},{111,112,107},{9,109,116},{109,32,115},{116,115,36},{109,115,116},
{32,106,118},{106,8,117},{118,117,35},{106,117,118},{36,119,121},{119,35,120},
{121,120,11},{119,120,121},{32,118,115},{118,35,119},{115,119,36},{118,119,115},
{10,122,124},{122,37,123},{124,123,39},{122,123,124},{37,125,127},{125,11,126},
{127,126,38},{125,126,127},{39,128,130},{128,38,129},{130,129,3},{128,129,130},
{37,127,123},{127,38,128},{123,128,39},{127,128,123},{11,125,132},{125,37,131},
{132,131,41},{125,131,132},{37,122,134},{122,10,133},{134,133,40},{122,133,134},
{41,135,137},{135,40,136},{137,136,5},{135,136,137},{37,134,131},{134,40,135},
{131,135,41},{134,135,131},{6,65,94},{65,18,138},{94,138,28},{65,138,94},
{18,62,139},{62,2,49},{139,49,13},{62,49,139},{28,140,97},{140,13,46},{97,46,0},
{140,46,97},{18,139,138},{139,13,140},{138,140,28},{139,140,138},{1,44,114},
{44,14,141},{114,141,34},{44,141,114},{14,50,142},{50,2,68},{142,68,21},
{50,68,142},{34,143,108},{143,21,73},{108,73,8},{143,73,108},{14,142,141},
{142,21,143},{141,143,34},{142,143,141},{0,52,98},{52,16,144},{98,144,29},
{52,144,98},{16,57,145},{57,4,76},{145,76,24},{57,76,145},{29,146,92},
{146,24,82},{92,82,7},{146,82,92},{16,145,144},{145,24,146},{144,146,29},
{145,146,144},{9,88,110},{88,25,147},{110,147,33},{88,147,110},{25,85,148},
{85,4,56},{148,56,15},{85,56,148},{33,149,113},{149,15,53},{113,53,1},
{149,53,113},{25,148,147},{148,15,149},{147,149,33},{148,149,147},{10,124,105},
{124,39,150},{105,150,31},{124,150,105},{39,130,151},{130,3,60},{151,60,19},
{130,60,151},{31,152,100},{152,19,66},{100,66,6},{152,66,100},{39,151,150},
{151,19,152},{150,152,31},{151,152,150},{8,72,117},{72,20,153},{117,153,35},
{72,153,117},{20,69,154},{69,3,129},{154,129,38},{69,129,154},{35,155,120},
{155,38,126},{120,126,11},{155,126,120},{20,154,153},{154,38,155},{153,155,35},
{154,155,153},{7,81,101},{81,23,156},{101,156,30},{81,156,101},{23,78,157},
{78,5,136},{157,136,40},{78,136,157},{30,158,104},{158,40,133},{104,133,10},
{158,133,104},{23,157,156},{157,40,158},{156,158,30},{157,158,156},{11,132,121},
{132,41,159},{121,159,36},{132,159,121},{41,137,160},{137,5,84},{160,84,26},
{137,84,160},{36,161,116},{161,26,89},{116,89,9},{161,89,116},{41,160,159},
{160,26,161},{159,161,36},{160,161,159}};
static int icosa_nbrs[320][3] =
{{3,208,21},{12,3,20},{14,209,3},{2,0,1},{7,12,17},{202,7,16},{201,13,7},
{6,4,5},{11,212,14},{198,11,13},{197,213,11},{10,8,9},{15,1,4},{9,15,6},
{8,2,15},{14,12,13},{19,224,5},{28,19,4},{30,225,19},{18,16,17},{23,28,1},
{250,23,0},{249,29,23},{22,20,21},{27,228,30},{246,27,29},{245,229,27},
{26,24,25},{31,17,20},{25,31,22},{24,18,31},{30,28,29},{35,261,53},{44,35,52},
{46,262,35},{34,32,33},{39,44,49},{197,39,48},{196,45,39},{38,36,37},
{43,265,46},{193,43,45},{192,266,43},{42,40,41},{47,33,36},{41,47,38},
{40,34,47},{46,44,45},{51,213,37},{60,51,36},{62,214,51},{50,48,49},{55,60,33},
{277,55,32},{276,61,55},{54,52,53},{59,217,62},{273,59,61},{272,218,59},
{58,56,57},{63,49,52},{57,63,54},{56,50,63},{62,60,61},{67,229,85},{76,67,84},
{78,230,67},{66,64,65},{71,76,81},{293,71,80},{292,77,71},{70,68,69},
{75,233,78},{289,75,77},{288,234,75},{74,72,73},{79,65,68},{73,79,70},
{72,66,79},{78,76,77},{83,309,69},{92,83,68},{94,310,83},{82,80,81},{87,92,65},
{245,87,64},{244,93,87},{86,84,85},{91,313,94},{241,91,93},{240,314,91},
{90,88,89},{95,81,84},{89,95,86},{88,82,95},{94,92,93},{99,234,117},
{108,99,116},{110,232,99},{98,96,97},{103,108,113},{192,103,112},{194,109,103},
{102,100,101},{107,226,110},{200,107,109},{202,224,107},{106,104,105},
{111,97,100},{105,111,102},{104,98,111},{110,108,109},{115,266,101},
{124,115,100},{126,264,115},{114,112,113},{119,124,97},{288,119,96},
{290,125,119},{118,116,117},{123,258,126},{296,123,125},{298,256,123},
{122,120,121},{127,113,116},{121,127,118},{120,114,127},{126,124,125},
{131,218,149},{140,131,148},{142,216,131},{130,128,129},{135,140,145},
{240,135,144},{242,141,135},{134,132,133},{139,210,142},{248,139,141},
{250,208,139},{138,136,137},{143,129,132},{137,143,134},{136,130,143},
{142,140,141},{147,314,133},{156,147,132},{158,312,147},{146,144,145},
{151,156,129},{272,151,128},{274,157,151},{150,148,149},{155,306,158},
{280,155,157},{282,304,155},{154,152,153},{159,145,148},{153,159,150},
{152,146,159},{158,156,157},{163,256,181},{172,163,180},{174,257,163},
{162,160,161},{167,172,177},{282,167,176},{281,173,167},{166,164,165},
{171,260,174},{278,171,173},{277,261,171},{170,168,169},{175,161,164},
{169,175,166},{168,162,175},{174,172,173},{179,304,165},{188,179,164},
{190,305,179},{178,176,177},{183,188,161},{298,183,160},{297,189,183},
{182,180,181},{187,308,190},{294,187,189},{293,309,187},{186,184,185},
{191,177,180},{185,191,182},{184,178,191},{190,188,189},{195,101,42},
{204,195,41},{206,102,195},{194,192,193},{199,204,38},{10,199,37},{9,205,199},
{198,196,197},{203,105,206},{6,203,205},{5,106,203},{202,200,201},{207,193,196},
{201,207,198},{200,194,207},{206,204,205},{211,138,0},{220,211,2},{222,136,211},
{210,208,209},{215,220,8},{48,215,10},{50,221,215},{214,212,213},{219,130,222},
{56,219,221},{58,128,219},{218,216,217},{223,209,212},{217,223,214},
{216,210,223},{222,220,221},{227,106,16},{236,227,18},{238,104,227},
{226,224,225},{231,236,24},{64,231,26},{66,237,231},{230,228,229},{235,98,238},
{72,235,237},{74,96,235},{234,232,233},{239,225,228},{233,239,230},
{232,226,239},{238,236,237},{243,133,90},{252,243,89},{254,134,243},
{242,240,241},{247,252,86},{26,247,85},{25,253,247},{246,244,245},{251,137,254},
{22,251,253},{21,138,251},{250,248,249},{255,241,244},{249,255,246},
{248,242,255},{254,252,253},{259,122,160},{268,259,162},{270,120,259},
{258,256,257},{263,268,168},{32,263,170},{34,269,263},{262,260,261},
{267,114,270},{40,267,269},{42,112,267},{266,264,265},{271,257,260},
{265,271,262},{264,258,271},{270,268,269},{275,149,58},{284,275,57},
{286,150,275},{274,272,273},{279,284,54},{170,279,53},{169,285,279},
{278,276,277},{283,153,286},{166,283,285},{165,154,283},{282,280,281},
{287,273,276},{281,287,278},{280,274,287},{286,284,285},{291,117,74},
{300,291,73},{302,118,291},{290,288,289},{295,300,70},{186,295,69},
{185,301,295},{294,292,293},{299,121,302},{182,299,301},{181,122,299},
{298,296,297},{303,289,292},{297,303,294},{296,290,303},{302,300,301},
{307,154,176},{316,307,178},{318,152,307},{306,304,305},{311,316,184},
{80,311,186},{82,317,311},{310,308,309},{315,146,318},{88,315,317},
{90,144,315},{314,312,313},{319,305,308},{313,319,310},{312,306,319},
{318,316,317}};


#ifdef TEST_DRIVER
VIEW  Current_View = {0,{0,0,0},{0,0,-1},{0,1,0},60,60,0};
int Pick_cnt =0;
int Pick_tri = -1,Picking = FALSE,Pick_samp=-1;
FVECT Pick_point[500],Pick_origin,Pick_dir;
FVECT  Pick_v0[500], Pick_v1[500], Pick_v2[500];
int    Pick_q[500];
FVECT P0,P1,P2;
FVECT FrustumNear[4],FrustumFar[4];
double dev_zmin=.01,dev_zmax=1000;
#endif

smDir(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    VSUB(ps,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm)); 
    normalize(ps);
}

smDir_in_cone(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    
    VSUB(ps,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm)); 
    normalize(ps);
}

smClear_flags(sm,which)
SM *sm;
int which;
{
  int i;

  if(which== -1)
    for(i=0; i < T_FLAGS;i++)
      bzero(SM_NTH_FLAGS(sm,i),FLAG_BYTES(SM_MAX_TRIS(sm)));
  else
    bzero(SM_NTH_FLAGS(sm,which),FLAG_BYTES(SM_MAX_TRIS(sm)));
}

/* Given an allocated mesh- initialize */
smInit_mesh(sm)
    SM *sm;
{
    /* Reset the triangle counters */
    SM_NUM_TRI(sm) = 0;
    SM_SAMPLE_TRIS(sm) = 0;
    SM_FREE_TRIS(sm) = -1;
    SM_AVAILABLE_TRIS(sm) = -1;
    smClear_flags(sm,-1);
}


/* Clear the SM for reuse: free any extra allocated memory:does initialization
   as well
*/
smClear(sm)
SM *sm;
{
  smClear_samples(sm);
  smClear_locator(sm);
  smInit_mesh(sm);
}

static int realloc_cnt=0;

int
smRealloc_mesh(sm)
SM *sm;
{
  int fbytes,i,max_tris,max_verts;
  
  if(realloc_cnt <=0)
    realloc_cnt = SM_MAX_TRIS(sm);

  max_tris = SM_MAX_TRIS(sm) + realloc_cnt;
  fbytes = FLAG_BYTES(max_tris);

  if(!(SM_TRIS(sm) = (TRI *)realloc(SM_TRIS(sm),max_tris*sizeof(TRI))))
    goto memerr;

  for(i=0; i< T_FLAGS; i++)
   if(!(SM_NTH_FLAGS(sm,i)=(int4 *)realloc((char *)SM_NTH_FLAGS(sm,i),fbytes)))
	goto memerr;

  SM_MAX_TRIS(sm) = max_tris;
  return(max_tris);

memerr:
	error(SYSTEM, "out of memory in smRealloc_mesh()");
}

/* Allocate and initialize a new mesh with max_verts and max_tris */
int
smAlloc_mesh(sm,max_verts,max_tris)
SM *sm;
int max_verts,max_tris;
{
    int fbytes,i;

    fbytes = FLAG_BYTES(max_tris);

    if(!(SM_TRIS(sm) = (TRI *)realloc(NULL,max_tris*sizeof(TRI))))
      goto memerr;

    if(!(SM_VERTS(sm) = (VERT *)realloc(NULL,max_verts*sizeof(VERT))))
      goto memerr;

    for(i=0; i< T_FLAGS; i++)
      if(!(SM_NTH_FLAGS(sm,i)=(int4 *)realloc(NULL,fbytes)))
	goto memerr;

    SM_MAX_VERTS(sm) = max_verts;
    SM_MAX_TRIS(sm) = max_tris;

    realloc_cnt = max_verts >> 1;

    smInit_mesh(sm);

    return(max_tris);
memerr:
	error(SYSTEM, "out of memory in smAlloc_mesh()");
}


int 
compress_set(os)
OBJECT *os;
{
  int i,j;

  for (i = 1, j = os[0]; i <= j; i++)
  {
    if(SM_T_ID_VALID(smMesh,os[i]))
      continue;
    if(i==j)
      break;
    while(!SM_T_ID_VALID(smMesh,os[j]) && (i < j))
	    j--;
    if(i==j)
      break;
    os[i] = os[j--];
  }
  i--;
	
  os[0] = i;
  return(i);

}

int
smAlloc_tri(sm)
SM *sm;
{
  int id;

  /* First check if there are any tris on the free list */
  /* Otherwise, have we reached the end of the list yet?*/
  if(SM_NUM_TRI(sm) < SM_MAX_TRIS(sm))
    return(SM_NUM_TRI(sm)++);
  if((id = SM_AVAILABLE_TRIS(sm)) != -1)
  {
    SM_AVAILABLE_TRIS(sm) =  T_NEXT_AVAILABLE(SM_NTH_TRI(sm,id));
    return(id);
  }
  if((id = SM_FREE_TRIS(sm)) != -1)
  {
    dosets(compress_set);
    SM_AVAILABLE_TRIS(sm) = T_NEXT_FREE(SM_NTH_TRI(sm,id));
    SM_FREE_TRIS(sm) = -1;
    return(id);
  }
  /*Else: realloc */
  smRealloc_mesh(sm);
  return(SM_NUM_TRI(sm)++);
}

smFree_mesh(sm)
SM *sm;
{
  int i;

  if(SM_TRIS(sm))
    free(SM_TRIS(sm));
  if(SM_VERTS(sm))
    free(SM_VERTS(sm));
  for(i=0; i< T_FLAGS; i++)
    if(SM_NTH_FLAGS(sm,i))
      free(SM_NTH_FLAGS(sm,i));
}

  
/* Initialize/clear global smL sample list for at least n samples */
smAlloc(max_samples)
   register int max_samples;
{
  unsigned nbytes;
  register unsigned i;
  int total_points;
  int max_tris,n;

  n = max_samples;
  /* If this is the first call, allocate sample,vertex and triangle lists */
  if(!smMesh)
  {
    if(!(smMesh = (SM *)malloc(sizeof(SM))))
       error(SYSTEM,"smAlloc():Unable to allocate memory\n");
    bzero(smMesh,sizeof(SM));
  }
  else
  {   /* If existing structure: first deallocate */
    smFree_mesh(smMesh);
    smFree_samples(smMesh);
    smFree_locator(smMesh);
 }

  /* First allocate at least n samples + extra points:at least enough
   necessary to form the BASE MESH- Default = 4;
  */
  SM_SAMP(smMesh) = sAlloc(&n,SM_EXTRA_POINTS);

  total_points = n + SM_EXTRA_POINTS;

  max_tris = total_points*2;
  /* Now allocate space for mesh vertices and triangles */
  max_tris = smAlloc_mesh(smMesh, total_points, max_tris);

  /* Initialize the structure for point,triangle location.
   */
  smAlloc_locator(smMesh);
}



smInit_sm(sm,vp)
SM *sm;
FVECT vp;
{

  smDist_sum = 0;
  smNew_tri_cnt = 0;

  VCOPY(SM_VIEW_CENTER(sm),vp);
  smInit_locator(sm,vp);
  smInit_samples(sm);
  smInit_mesh(sm);  
  smCreate_base_mesh(sm,SM_DEFAULT);
}

/*
 * int
 * smInit(n)		: Initialize/clear data structures for n entries
 * int	n;
 *
 * This routine allocates/initializes the sample, mesh, and point-location
 * structures for at least n samples.
 * If n is <= 0, then clear data structures.  Returns number samples
 * actually allocated.
 */

int
smInit(n)
   register int	n;
{
  int max_vertices;


  /* If n <=0, Just clear the existing structures */
  if(n <= 0)
  {
    smClear(smMesh);
    return(0);
  }

  /* Total mesh vertices includes the sample points and the extra vertices
     to form the base mesh
  */
  max_vertices = n + SM_EXTRA_POINTS;

  /* If the current mesh contains enough room, clear and return */
  if(smMesh && (max_vertices <= SM_MAX_VERTS(smMesh)) && SM_MAX_SAMP(smMesh) <=
     n && SM_MAX_POINTS(smMesh) <= max_vertices)
  {
    smClear(smMesh);
    return(SM_MAX_SAMP(smMesh));
  }
  /* Otherwise- mesh must be allocated with the appropriate number of
    samples
  */
  smAlloc(n);

  return(SM_MAX_SAMP(smMesh));
}


smLocator_apply(sm,v0,v1,v2,func)
   SM *sm;
   FVECT v0,v1,v2;
   FUNC func;
{
  STREE *st;
  FVECT tri[3];

  st = SM_LOCATOR(sm);

  VSUB(tri[0],v0,SM_VIEW_CENTER(sm));
  VSUB(tri[1],v1,SM_VIEW_CENTER(sm));
  VSUB(tri[2],v2,SM_VIEW_CENTER(sm));
  stVisit(st,tri,func);

}


/* NEW INSERTION!!! ******************************************************/
QUADTREE
insert_tri(argptr,root,qt,q0,q1,q2,t0,t1,t2,dt10,dt21,dt02,scale,
               s0,s1,s2,sq0,sq1,sq2,rev,f,n)
     int *argptr;
     int root;
     QUADTREE qt;
     BCOORD q0[3],q1[3],q2[3];
     BCOORD t0[3],t1[3],t2[3];
     BCOORD dt10[3],dt21[3],dt02[3];
     unsigned int scale,s0,s1,s2,sq0,sq1,sq2,rev;
     FUNC f; 
     int n;
{
  OBJECT *optr,*tptr,t_set[QT_MAXSET+1];
  int i,t_id;
  TRI *t;
  FVECT tri[3];
  BCOORD b0[3],b1[3],b2[3];
  BCOORD tb0[3],tb1[3],tb2[3];
  BCOORD db10[3],db21[3],db02[3];
  BCOORD a,b,c,qa,qb,qc;
  FUNC tfunc;

  t_id = *argptr;

  /* If the set is empty - just add */
  if(QT_IS_EMPTY(qt))
    return(qtadduelem(qt,t_id));

  /* If the set size is below expansion threshold,or at maximum
     number of quadtree levels : just add */
  optr = qtqueryset(qt);
  if(QT_SET_CNT(optr) < QT_SET_THRESHOLD || (n > (QT_MAX_LEVELS-2)))
    return(qtadduelem(qt,t_id));
  /* otherwise: expand node- and insert tri, and reinsert existing tris
     in set to children of new node
  */
  /* First tri compressing set */
#ifdef NEWSETS0
  if(qtcompressuelem(qt,compress_set) < QT_SET_THRESHOLD)
    /* Get set: If greater than standard size: allocate memory to hold */
    return(qtadduelem(qt,t_id));
#endif
    if(QT_SET_CNT(optr) > QT_MAXSET)
    { 
       if(!(tptr = (OBJECT *)malloc((QT_SET_CNT(optr)+1)*sizeof(OBJECT))))
        goto memerr;
    }
    else
     tptr = t_set;
    qtgetset(tptr,qt);

  /* subdivide node */
  qtfreeleaf(qt);
  qtSubdivide(qt);

  a = q1[0];  b = q0[1]; c = q0[2];
  qa = q0[0]; qb = q1[1];  qc = q2[2];
  /* First add current tri: have all of the information */
  if(rev)
    qt = qtInsert_tri_rev(root,qt,q0,q1,q2,t0,t1,t2,dt10,dt21,dt02,scale,
               s0,s1,s2,sq0,sq1,sq2,f,n);
  else
    qt = qtInsert_tri(root,qt,q0,q1,q2,t0,t1,t2,dt10,dt21,dt02,scale,
               s0,s1,s2,sq0,sq1,sq2,f,n);

  /* Now add in all of the rest; */
  F_FUNC(tfunc) = F_FUNC(f);
  for(optr = QT_SET_PTR(tptr),i=QT_SET_CNT(tptr); i > 0; i--)
  {
    t_id = QT_SET_NEXT_ELEM(optr);
    t = SM_NTH_TRI(smMesh,t_id);
    if(!T_IS_VALID(t))
      continue;
    F_ARGS(tfunc) = &t_id;
    VSUB(tri[0],SM_T_NTH_WV(smMesh,t,0),SM_VIEW_CENTER(smMesh));
    VSUB(tri[1],SM_T_NTH_WV(smMesh,t,1),SM_VIEW_CENTER(smMesh));
    VSUB(tri[2],SM_T_NTH_WV(smMesh,t,2),SM_VIEW_CENTER(smMesh));
    convert_tri_to_frame(root,tri,b0,b1,b2,db10,db21,db02);

    /* Calculate appropriate sidedness values */
    SIDES_GTR(b0,b1,b2,s0,s1,s2,a,b,c);
    SIDES_GTR(b0,b1,b2,sq0,sq1,sq2,qa,qb,qc);
    if(rev)
      qt = qtInsert_tri_rev(root,qt,q0,q1,q2,b0,b1,b2,db10,db21,db02,scale,
               s0,s1,s2,sq0,sq1,sq2,tfunc,n);
    else
      qt = qtInsert_tri(root,qt,q0,q1,q2,b0,b1,b2,db10,db21,db02,scale,
               s0,s1,s2,sq0,sq1,sq2,tfunc,n);
  }
  /* If we allocated memory: free it */
  if( tptr != t_set)
    free(tptr);

  return(qt);
memerr:
    error(SYSTEM,"expand_node():Unable to allocate memory");

}



smLocator_add_tri(sm,t_id,v0_id,v1_id,v2_id)
SM *sm;
int t_id;
int v0_id,v1_id,v2_id;
{
  FVECT tri[3];
  FUNC f;

#ifdef DEBUG
if(T_NTH_NBR(SM_NTH_TRI(sm,t_id),0)== -1 ||
   T_NTH_NBR(SM_NTH_TRI(sm,t_id),1)== -1 ||
   T_NTH_NBR(SM_NTH_TRI(sm,t_id),2)== -1)
  error("Invalid tri: smLocator_add_tri()\n");

if(!T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t_id),0))) ||
   !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t_id),1))) ||
   !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,t_id),2))))
  error("Invalid tri: smLocator_add_tri()\n");
#endif

  VSUB(tri[0],SM_NTH_WV(sm,v0_id),SM_VIEW_CENTER(sm));
  VSUB(tri[1],SM_NTH_WV(sm,v1_id),SM_VIEW_CENTER(sm));
  VSUB(tri[2],SM_NTH_WV(sm,v2_id),SM_VIEW_CENTER(sm));

  F_FUNC(f) = insert_tri;
  F_ARGS(f) = &t_id;
  stInsert_tri(SM_LOCATOR(sm),tri,f);
}

/* Add a triangle to the base array with vertices v1-v2-v3 */
int
smAdd_tri(sm, v0_id,v1_id,v2_id)
SM *sm;
int v0_id,v1_id,v2_id;
{
    int t_id;
    TRI *t;
#ifdef DEBUG
    if(v0_id==v1_id || v0_id==v2_id || v1_id==v2_id)
      error(CONSISTENCY,"smAdd_tri: invalid vertex ids\n");
#endif    
    t_id = smAlloc_tri(sm);

    if(t_id == -1)
      return(t_id);

    t = SM_NTH_TRI(sm,t_id);

    T_CLEAR_NBRS(t);
    /* set the triangle vertex ids */
    T_NTH_V(t,0) = v0_id;
    T_NTH_V(t,1) = v1_id;
    T_NTH_V(t,2) = v2_id;

    SM_NTH_VERT(sm,v0_id) = t_id;
    SM_NTH_VERT(sm,v1_id) = t_id;
    SM_NTH_VERT(sm,v2_id) = t_id;

    if(SM_BASE_ID(sm,v0_id) || SM_BASE_ID(sm,v1_id) || SM_BASE_ID(sm,v2_id))
      SM_SET_NTH_T_BASE(sm,t_id);
    else
      if(SM_DIR_ID(sm,v0_id) && SM_DIR_ID(sm,v1_id) && SM_DIR_ID(sm,v2_id))
      SM_SET_NTH_T_BG(sm,t_id);

    S_SET_FLAG(T_NTH_V(t,0));
    S_SET_FLAG(T_NTH_V(t,1));
    S_SET_FLAG(T_NTH_V(t,2));

    SM_SET_NTH_T_ACTIVE(sm,t_id);
    SM_SET_NTH_T_NEW(sm,t_id);

    SM_SAMPLE_TRIS(sm)++;
    smNew_tri_cnt++;

    /* return initialized triangle */
    return(t_id);
}


void
smTris_swap_edge(sm,t_id,t1_id,e,e1,tn_id,tn1_id,add_ptr)
   SM *sm;
   int t_id,t1_id;
   int e,e1;
   int *tn_id,*tn1_id;
   LIST **add_ptr;
{
    int verts[3],enext,eprev,e1next,e1prev;
    TRI *n,*ta,*tb,*t,*t1;
    FVECT p1,p2,p3;
    int ta_id,tb_id;
    /* form new diagonal (e relative to t, and e1 relative to t1)
      defined by quadrilateral formed by t,t1- swap for the opposite diagonal
   */
    enext = (e+1)%3;
    eprev = (e+2)%3;
    e1next = (e1+1)%3;
    e1prev = (e1+2)%3;
    verts[e] = T_NTH_V(SM_NTH_TRI(sm,t_id),e);
    verts[enext] = T_NTH_V(SM_NTH_TRI(sm,t_id),enext);
    verts[eprev] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1);
    ta_id = smAdd_tri(sm,verts[0],verts[1],verts[2]);
    *add_ptr = push_data(*add_ptr,ta_id);
    verts[e1] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1);
    verts[e1next] = T_NTH_V(SM_NTH_TRI(sm,t1_id),e1next);
    verts[e1prev] = T_NTH_V(SM_NTH_TRI(sm,t_id),e);
    tb_id = smAdd_tri(sm,verts[0],verts[1],verts[2]);

    *add_ptr = push_data(*add_ptr,tb_id);
    ta = SM_NTH_TRI(sm,ta_id);
    tb = SM_NTH_TRI(sm,tb_id);
    t = SM_NTH_TRI(sm,t_id);
    t1 = SM_NTH_TRI(sm,t1_id);
    /* set the neighbors */
    T_NTH_NBR(ta,e) = T_NTH_NBR(t1,e1next);
    T_NTH_NBR(tb,e1) = T_NTH_NBR(t,enext);
    T_NTH_NBR(ta,enext)= tb_id;
    T_NTH_NBR(tb,e1next)= ta_id;
    T_NTH_NBR(ta,eprev)=T_NTH_NBR(t,eprev);
    T_NTH_NBR(tb,e1prev)=T_NTH_NBR(t1,e1prev);    

  
#ifdef DEBUG
	if(!T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(ta,0))) || 
		       !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(ta,1))) || 
		       !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(ta,2))) || 
		       !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(tb,0))) || 
		       !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(tb,1))) || 
		       !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(tb,2))))
	  goto Ltri_error;
	/*
	if(SM_NTH_TRI(sm,T_NTH_NBR(ta,0))==SM_NTH_TRI(sm,T_NTH_NBR(ta,1)) ||
	   SM_NTH_TRI(sm,T_NTH_NBR(ta,0))==SM_NTH_TRI(sm,T_NTH_NBR(ta,2)) ||
	   SM_NTH_TRI(sm,T_NTH_NBR(ta,1))==SM_NTH_TRI(sm,T_NTH_NBR(ta,2)) ||
	   SM_NTH_TRI(sm,T_NTH_NBR(tb,0))==SM_NTH_TRI(sm,T_NTH_NBR(tb,1)) ||
	   SM_NTH_TRI(sm,T_NTH_NBR(tb,0))==SM_NTH_TRI(sm,T_NTH_NBR(tb,2)) ||
	   SM_NTH_TRI(sm,T_NTH_NBR(tb,1))==SM_NTH_TRI(sm,T_NTH_NBR(tb,2)) )
	  goto Ltri_error;
	  */
#endif
    /* Reset neighbor pointers of original neighbors */
    n = SM_NTH_TRI(sm,T_NTH_NBR(t,enext));
#ifdef DEBUG
	if(!T_IS_VALID(n))
	  goto Ltri_error;
#endif
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = tb_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(t,eprev));
#ifdef DEBUG
	if(!T_IS_VALID(n))
	  goto Ltri_error;
#endif
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = ta_id;

    n = SM_NTH_TRI(sm,T_NTH_NBR(t1,e1next));
#ifdef DEBUG
	if(!T_IS_VALID(n))
	  goto Ltri_error;
#endif
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = ta_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(t1,e1prev));
#ifdef DEBUG
	if(!T_IS_VALID(n))
	  goto Ltri_error;
#endif
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = tb_id;

#ifdef DEBUG
    T_VALID_FLAG(SM_NTH_TRI(sm,t_id))=-1;
    T_VALID_FLAG(SM_NTH_TRI(sm,t1_id))=-1;
#endif

    remove_from_list(t_id,add_ptr);
    remove_from_list(t1_id,add_ptr);

    smDelete_tri(sm,t_id); 
    smDelete_tri(sm,t1_id); 

    *tn_id = ta_id;
    *tn1_id = tb_id;
    
#ifdef DEBUG
    if(T_NTH_NBR(SM_NTH_TRI(sm,ta_id),0)== -1 ||
       T_NTH_NBR(SM_NTH_TRI(sm,ta_id),1)== -1 ||
       T_NTH_NBR(SM_NTH_TRI(sm,ta_id),2)== -1 ||
       T_NTH_NBR(SM_NTH_TRI(sm,tb_id),0)== -1 ||
       T_NTH_NBR(SM_NTH_TRI(sm,tb_id),1)== -1 ||
       T_NTH_NBR(SM_NTH_TRI(sm,tb_id),2)== -1)
      goto Ltri_error;
  if(!T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,ta_id),0))) ||
     !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,ta_id),1))) ||
     !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,ta_id),2))))
      goto Ltri_error;
  if(!T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,tb_id),0))) ||
     !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,tb_id),1))) ||
     !T_IS_VALID(SM_NTH_TRI(sm,T_NTH_NBR(SM_NTH_TRI(sm,tb_id),2))))
      goto Ltri_error;
#endif
    return;
Ltri_error:
    error(CONSISTENCY,"Invalid tri: smTris_swap_edge()\n");
}

smUpdate_locator(sm,add_list)
SM *sm;
LIST *add_list;
{
  int t_id;
  TRI *t;
  
  while(add_list)
  {
    t_id = pop_list(&add_list);
    t = SM_NTH_TRI(sm,t_id);
    smLocator_add_tri(sm,t_id,T_NTH_V(t,0),T_NTH_V(t,1),T_NTH_V(t,2));
  }
}

int
smFix_tris(sm,id,tlist,add_list)
SM *sm;
int id;
LIST *tlist,*add_list;
{
    TRI *t,*t_opp;
    FVECT p,p0,p1,p2;
    int e,e1,swapped = 0;
    int t_id,t_opp_id;
    LIST *del_list=NULL,*lptr;

    VSUB(p,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
    while(tlist)
    {
	t_id = pop_list(&tlist);
#ifdef DEBUG
	if(t_id==INVALID || t_id > smMesh->num_tri)
	  error(CONSISTENCY,"Invalid tri id smFix_tris()\n");
#endif
        t = SM_NTH_TRI(sm,t_id);
        e = T_WHICH_V(t,id);
        t_opp_id = T_NTH_NBR(t,e);
#ifdef DEBUG
	if(t_opp_id==INVALID || t_opp_id > smMesh->num_tri)
	  error(CONSISTENCY,"Invalid tri id smFix_tris()\n");
#endif
	t_opp = SM_NTH_TRI(sm,t_opp_id);
#ifdef DEBUG
	if(!T_IS_VALID(t_opp))
	  error(CONSISTENCY,"Invalid trismFix_tris()\n");
#endif
	smDir_in_cone(sm,p0,T_NTH_V(t_opp,0));
	smDir_in_cone(sm,p1,T_NTH_V(t_opp,1));
	smDir_in_cone(sm,p2,T_NTH_V(t_opp,2));
	if(point_in_cone(p,p0,p1,p2))
	{
	    swapped = 1;
	    e1 = T_NTH_NBR_PTR(t_id,t_opp);
	    /* check list for t_opp and Remove if there */
	    remove_from_list(t_opp_id,&tlist);
	    smTris_swap_edge(sm,t_id,t_opp_id,e,e1,&t_id,&t_opp_id,
			     &add_list);
	    tlist = push_data(tlist,t_id);
	    tlist = push_data(tlist,t_opp_id);
	}
    }

    smUpdate_locator(sm,add_list);

    return(swapped);
}

/* Give the vertex "id" and a triangle "t" that it belongs to- return the
   next nbr in a counter clockwise order about vertex "id"
*/
int
smTri_next_ccw_nbr(sm,t,id)
SM *sm;
TRI *t;
int id;
{
  int which;
  int nbr_id;
  
  /* Want the edge for which "id" is the destination */
  which = T_WHICH_V(t,id);
  if(which== INVALID)
    return(which);

  nbr_id = T_NTH_NBR(t,(which + 1)% 3);
  return(nbr_id);
}

smClear_tri_flags(sm,id)
SM *sm;
int id;
{
  int i;

  for(i=0; i < T_FLAGS; i++)
    SM_CLR_NTH_T_FLAG(sm,id,i);

}

/* Locate the point-id in the point location structure: */
int
smInsert_samp(sm,s_id,tri_id,on,which)
   SM *sm;
   int s_id,tri_id;
   int on,which;
{
    int v_id[3],topp_id,i;
    int t0_id,t1_id,t2_id,t3_id,replaced;
    int e0,e1,e2,opp0,opp1,opp2,opp_id;
    LIST *tlist,*add_list;
    FVECT npt;
    TRI *tri,*nbr,*topp;

    add_list = NULL;
    for(i=0; i<3;i++)
      v_id[i] = T_NTH_V(SM_NTH_TRI(sm,tri_id),i);

    /* If falls on existing edge */
    if(on == ON_E)
    {
      tri = SM_NTH_TRI(sm,tri_id);
      topp_id = T_NTH_NBR(tri,which);
      e0 = which;
      e1 = (which+1)%3;
      e2 = (which+2)%3;
      topp = SM_NTH_TRI(sm,topp_id);
      opp0 = T_NTH_NBR_PTR(tri_id,topp);
      opp_id = T_NTH_V(topp,opp0);
      opp1 = (opp0+1)%3;
      opp2 = (opp0+2)%3;
      
      /* Create 4 triangles */
      /*      /|\             /|\
             / | \           / | \
	    /  *  \  ---->  /__|__\
            \  |  /         \  |  /
	     \ | /           \ | /
	      \|/             \|/
     */
	t0_id = smAdd_tri(sm,s_id,v_id[e0],v_id[e1]);
	add_list = push_data(add_list,t0_id);
	t1_id = smAdd_tri(sm,s_id,v_id[e2],v_id[e0]);
	add_list = push_data(add_list,t1_id);
	t2_id = smAdd_tri(sm,s_id,v_id[e1],opp_id);
	add_list = push_data(add_list,t2_id);
	t3_id = smAdd_tri(sm,s_id,opp_id,v_id[e2]);
	add_list = push_data(add_list,t3_id);
	/* CAUTION: tri must be assigned after Add_tri: pointers may change*/
	tri = SM_NTH_TRI(sm,tri_id);
	topp =SM_NTH_TRI(sm,topp_id);

	/* Set the neighbor pointers for the new tris */
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),0) = T_NTH_NBR(tri,e2);
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),1) = t2_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),2) = t1_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),0) = T_NTH_NBR(tri,e1);
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),1) = t0_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),2) = t3_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),0) = T_NTH_NBR(topp,opp1);
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),1) = t3_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),2) = t0_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t3_id),0) = T_NTH_NBR(topp,opp2);
	T_NTH_NBR(SM_NTH_TRI(sm,t3_id),1) = t1_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t3_id),2) = t2_id;

	/* Reset the neigbor pointers for the neighbors of the original */
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,e1));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t1_id;
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,e2));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t0_id;
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(topp,opp1));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(topp_id,nbr)) = t2_id;
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(topp,opp2));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(topp_id,nbr)) = t3_id;
#ifdef DEBUG
    T_VALID_FLAG(SM_NTH_TRI(sm,tri_id))=-1;
    T_VALID_FLAG(SM_NTH_TRI(sm,topp_id))=-1;
#endif
	tlist = push_data(NULL,t0_id);
	tlist = push_data(tlist,t1_id);
	tlist = push_data(tlist,t2_id);
	tlist = push_data(tlist,t3_id);
    }
    else
      {
	/* Create 3 triangles */
      /*      / \             /|\
             /   \           / | \
	    /  *  \  ---->  /  |  \
           /       \       /  / \  \
	  /         \ 	  / /     \ \ 
	 /___________\   //_________\\ 
      */

	t0_id = smAdd_tri(sm,s_id,v_id[0],v_id[1]);
	add_list = push_data(add_list,t0_id);
	t1_id = smAdd_tri(sm,s_id,v_id[1],v_id[2]);
	add_list = push_data(add_list,t1_id);
	t2_id = smAdd_tri(sm,s_id,v_id[2],v_id[0]);
	add_list = push_data(add_list,t2_id);

	/* CAUTION: tri must be assigned after Add_tri: pointers may change*/
	tri = SM_NTH_TRI(sm,tri_id);
	/* Set the neighbor pointers for the new tris */
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),0) = T_NTH_NBR(tri,2);
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),1) = t1_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t0_id),2) = t2_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),0) = T_NTH_NBR(tri,0);
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),1) = t2_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t1_id),2) = t0_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),0) = T_NTH_NBR(tri,1);
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),1) = t0_id;
	T_NTH_NBR(SM_NTH_TRI(sm,t2_id),2) = t1_id;
	
	/* Reset the neigbor pointers for the neighbors of the original */
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,0));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t1_id;
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,1));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t2_id;
	nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,2));
	T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t0_id;

	tlist = push_data(NULL,t0_id);
	tlist = push_data(tlist,t1_id);
	tlist = push_data(tlist,t2_id);
      }
    /* Fix up the new triangles*/
    smFix_tris(sm,s_id,tlist,add_list);

    smDelete_tri(sm,tri_id);
    if(on==ON_E)
      smDelete_tri(sm,topp_id);

    return(TRUE);

#ifdef DEBUG
Ladderror:
        error(CONSISTENCY,"Invalid tri: smInsert_samp()\n");
#endif
}    

int
smTri_in_set(sm,p,optr,onptr,whichptr)
SM *sm;
FVECT p;
OBJECT *optr;
int *onptr,*whichptr;
{
  int i,t_id,on0,on1,on2;
  FVECT n,v0,v1,v2;
  TRI *t;
  double side;

  for (i = QT_SET_CNT(optr),optr = QT_SET_PTR(optr);i > 0; i--)
  {
    /* Find the first triangle that pt falls */
    t_id = QT_SET_NEXT_ELEM(optr);
    t = SM_NTH_TRI(sm,t_id);
    if(!T_IS_VALID(t))
      continue;
    VSUB(v0,SM_T_NTH_WV(sm,t,0),SM_VIEW_CENTER(sm));
    VSUB(v1,SM_T_NTH_WV(sm,t,1),SM_VIEW_CENTER(sm));
    VSUB(v2,SM_T_NTH_WV(sm,t,2),SM_VIEW_CENTER(sm));

    if(EQUAL_VEC3(v0,p))
    {
      *onptr = ON_V;
      *whichptr = 0;
      return(t_id);
    }
    if(EQUAL_VEC3(v1,p))
    {
      *onptr = ON_V;
      *whichptr = 1;
      return(t_id);
    }
    if(EQUAL_VEC3(v2,p))
    {
      *onptr = ON_V;
      *whichptr = 2;
      return(t_id);
    }
    
    on0 = on1 =on2 = 0;
    VCROSS(n,v0,v1);
    side = DOT(n,p);
    if(ZERO(side))
      on2 = TRUE;
    else 
      if(side >0.0)
	continue;

    VCROSS(n,v1,v2);
    side = DOT(n,p);
    if(ZERO(side))
      on0 = TRUE;
    else 
      if(side >0.0)
	continue;

    VCROSS(n,v2,v0);
    side = DOT(n,p);
    if(ZERO(side))
      on1 = TRUE;
    else 
      if(side >0.0)
	continue;
    if(on0)
    {
      if(on1)
      {
	*onptr = ON_P;
	*whichptr = 2;
      }
      else 
	if(on2)
       {
	 *onptr = ON_P;
	 *whichptr = 1;
       }
	else
	  {
	    *onptr = ON_E;
	    *whichptr = 0;
	  }
      return(t_id);
    }
   if(on1)
    {
      if(on2)
      {
	*onptr = ON_P;
	*whichptr = 0;
      }
      else
      {
	*onptr = ON_E;
	*whichptr = 1;
      }
      return(t_id);
    }
   if(on2)
   {
     *onptr = ON_E;
     *whichptr = 2;
      return(t_id);
   }
    *onptr = IN_T;
    return(t_id);
  }
  return(INVALID);
}

int
smPointLocateTri(sm,p,onptr,whichptr)
SM *sm;
FVECT p;
int *onptr,*whichptr;
{
  QUADTREE qt,*optr;
  FVECT tpt;
  int tri_id;

  VSUB(tpt,p,SM_VIEW_CENTER(sm));
  qt = smPointLocateCell(sm,tpt);
  if(QT_IS_EMPTY(qt))
    return(INVALID);  
 
  optr = qtqueryset(qt);
  tri_id = smTri_in_set(sm,tpt,optr,onptr,whichptr);

  return(tri_id);
}


/*
   Determine whether this sample should:
   a) be added to the mesh by subdividing the triangle
   b) ignored
   c) replace values of an existing mesh vertex

   In case a, the routine will return TRUE, for b,c will return FALSE
   In case a, also determine if this sample should cause the deletion of
   an existing vertex. If so *rptr will contain the id of the sample to delete
   after the new sample has been added.

   ASSUMPTION: this will not be called with a new sample that is
               a BASE point.

   The following tests are performed (in order) to determine the fate
   of the sample:

   1) If the new sample is close in ws, and close in the spherical projection
      to one of the triangle vertex samples:
         pick the point with dir closest to that of the canonical view
	 If it is the new point: mark existing point for deletion,and return
	 TRUE,else return FALSE
   2) If the spherical projection of new is < REPLACE_EPS from a base point:
      add new and mark the base for deletion: return TRUE
   3) If the addition of the new sample point would introduce a "puncture"
      or cause new triangles with large depth differences:return FALSE
     This attempts to throw out points that should be occluded   
*/
int
smTest_sample(sm,tri_id,dir,p,rptr)
   SM *sm;
   int tri_id;
   FVECT dir,p;
   int *rptr;
{
    TRI *tri;
    double d,d2,dnear,dspt,d01,d12,d20,s0,s1,s2,ds,dv;
    int vid[3],i,nearid,norm,dcnt,bcnt;
    FVECT diff[3],spt,npt;

    *rptr = INVALID;
    bcnt = dcnt = 0;
    tri = SM_NTH_TRI(sm,tri_id);
    vid[0] = T_NTH_V(tri,0);
    vid[1] = T_NTH_V(tri,1);
    vid[2] = T_NTH_V(tri,2);

    for(i=0; i<3; i++)
    {
      if(SM_BASE_ID(sm,vid[i]))
	bcnt++;
      if(SM_DIR_ID(sm,vid[i]))
	dcnt++;
      
    }
    /* TEST 1: If the new sample is close in ws, and close in the spherical
       projection to one of the triangle vertex samples
    */
#if 0
    norm = FALSE;
      VSUB(spt,p,SM_VIEW_CENTER(sm));
      ds = DOT(spt,spt);
      dnear = FHUGE;
      for(i=0; i<3; i++)
	{
	  d = DOT(diff[i],diff[i])/ds;
	  if(d < dnear)
	    {
	      dnear = d;
	      nearid=vid[i];
	    }
	}

      if(dnear <=  SM_MIN_SAMP_DIFF*SM_MIN_SAMP_DIFF)
	{
	  /* Pick the point with dir closest to that of the canonical view
	     if it is the new sample: mark existing point for deletion
	     */
	  if(SM_BASE_ID(sm,nearid))
	  {
	    *rptr = nearid;
	    return(TRUE);
	  }
	  if(SM_DIR_ID(sm,nearid))
	    return(FALSE);
	  if(!dir)
	  {
	    *rptr = nearid;
	    return(TRUE);
	  }
	  normalize(spt);
	  norm = TRUE;
	  VSUB(npt,SM_NTH_WV(sm,nearid),SM_VIEW_CENTER(sm));
	  normalize(npt);
	  d = fdir2diff(SM_NTH_W_DIR(sm,nearid), npt);
	  d2 = 2. - 2.*DOT(dir,spt);
	  /* The existing sample is a better sample:punt */
	 if(d2 > d)
	    return(FALSE);
	 else
	 {
	/* The new sample is better: mark the existing one
		   for deletion after the new one is added*/
	*rptr = nearid;
	return(TRUE);
	}
     }	
    
  /* TEST 3: If the spherical projection of new is < S_REPLACE_EPS
     from a base point: Edge length is constrained to subtend <45 degrees:
     original base mesh edges are approx 32 degrees- so have about 13 degrees
     to work in: S_REPLACE_EPS is the square of the radian value
  */

    if(bcnt)
    {   
	dnear = FHUGE;
	if(!norm)
	  normalize(spt);

	for(i=0; i<3; i++)
	{
	    if(!SM_BASE_ID(sm,vid[i]))
	       continue;
	    VSUB(npt,SM_NTH_WV(sm,vid[i]),SM_VIEW_CENTER(sm));
	    d = DIST_SQ(npt,spt);
	    if(dnear <=  SM_MIN_SAMP_DIFF*SM_MIN_SAMP_DIFF && d< near)
	       {
		   dnear = d;
		   nearid = vid[i];
	       }
	}
	if(dnear != FHUGE)
	{
	    /* add new and mark the base for deletion */
	    *rptr = nearid;
	    return(TRUE);
	}
    }
#else
    {
      FVECT nearpt;
    dnear = FHUGE;
    VSUB(spt,p,SM_VIEW_CENTER(sm));
    ds = DOT(spt,spt);
    normalize(spt);

    for(i=0; i<3; i++)
    {
	
      VSUB(npt,SM_NTH_WV(sm,vid[i]),SM_VIEW_CENTER(sm));
      
      if(!SM_BASE_ID(sm,vid[i]) && !SM_DIR_ID(sm,vid[i]))
	normalize(npt);

      d = DIST_SQ(npt,spt);
      if(d < SM_MIN_SAMP_DIFF*SM_MIN_SAMP_DIFF && d < dnear)
	{
	  dnear = d;
	  nearid = vid[i];
	  VCOPY(nearpt,npt);
	}

    }
    if(dnear != FHUGE)
    {
	  /* Pick the point with dir closest to that of the canonical view
	     if it is the new sample: mark existing point for deletion
	     */
	  if(SM_BASE_ID(sm,nearid))
	  {
	    *rptr = nearid;
	    return(TRUE);
	  }
	  if(SM_DIR_ID(sm,nearid))
	    return(FALSE);
	  if(!dir)
	  {
	    *rptr = nearid;
	    return(TRUE);
	  }
	  d = fdir2diff(SM_NTH_W_DIR(sm,nearid), nearpt);
	  d2 = 2. - 2.*DOT(dir,spt);
	  /* The existing sample is a better sample:punt */
	 if(d2 > d)
	    return(FALSE);
	 else
	 {
	/* The new sample is better: mark the existing one
		   for deletion after the new one is added*/
	*rptr = nearid;
	return(TRUE);
	}
    }	
    }
#endif	
  /* TEST 4:
     If the addition of the new sample point would introduce a "puncture"
     or cause new triangles with large depth differences:dont add:    
     */
    if(bcnt || dcnt)
       return(TRUE);

    /* If the new point is in front of the existing points- add */
    dv = DIST_SQ(SM_NTH_WV(sm,vid[0]),SM_VIEW_CENTER(sm));
    if(ds < dv)
      return(TRUE);

    d01 = DIST_SQ(SM_NTH_WV(sm,vid[1]),SM_NTH_WV(sm,vid[0]));
    VSUB(diff[0],SM_NTH_WV(sm,vid[0]),p);
    s0 = DOT(diff[0],diff[0]);
    if(s0 < S_REPLACE_SCALE*d01)
       return(TRUE);

    d12 = DIST_SQ(SM_NTH_WV(sm,vid[2]),SM_NTH_WV(sm,vid[1]));
    if(s0 < S_REPLACE_SCALE*d12)
       return(TRUE);    
    d20 = DIST_SQ(SM_NTH_WV(sm,vid[0]),SM_NTH_WV(sm,vid[2]));
    if(s0 < S_REPLACE_SCALE*d20)
       return(TRUE);    
    d = MIN3(d01,d12,d20);
    VSUB(diff[1],SM_NTH_WV(sm,vid[1]),p); 
   s1 = DOT(diff[1],diff[1]);
    if(s1 < S_REPLACE_SCALE*d)
       return(TRUE);
    VSUB(diff[2],SM_NTH_WV(sm,vid[2]),p);
    s2 = DOT(diff[2],diff[2]);
    if(s2 < S_REPLACE_SCALE*d)
       return(TRUE);    

  /* TEST 5:
     Check to see if triangle is relatively large and should therefore
     be subdivided anyway.
     */
    dv *= DIST_SQ(SM_NTH_WV(sm,vid[1]),SM_VIEW_CENTER(sm));
    dv *= DIST_SQ(SM_NTH_WV(sm,vid[2]),SM_VIEW_CENTER(sm));
    if (d01*d12*d20/dv > S_REPLACE_TRI)
	return(TRUE);
	    
    return(FALSE);
}

smReplace_base_samp(sm,b_id,s_id,tri,t_id,which)
SM *sm;
int b_id,s_id;
TRI *tri;
int t_id,which;
{
  TRI *t;

  SM_NTH_VERT(sm,s_id) = t_id;
  T_NTH_V(tri,which) = s_id;
  if(!(SM_BASE_ID(sm,T_NTH_V(tri,(which+1)%3)) || 
       SM_BASE_ID(sm,T_NTH_V(tri,(which+2)%3)))) 
    SM_CLR_NTH_T_BASE(sm,t_id);
  if(!SM_IS_NTH_T_NEW(sm,t_id))
  {
    smNew_tri_cnt++;
    SM_SET_NTH_T_NEW(sm,t_id);
  }
  t_id = smTri_next_ccw_nbr(sm,tri,b_id);
  while(t_id != INVALID)
  {
    t = SM_NTH_TRI(sm,t_id);
    which = T_WHICH_V(t,b_id);
    T_NTH_V(t,which) = s_id;
    /* Check if still a base triangle */
    if(!(SM_BASE_ID(sm,T_NTH_V(t,(which+1)%3)) || 
	 SM_BASE_ID(sm,T_NTH_V(t,(which+2)%3)))) 
      SM_CLR_NTH_T_BASE(sm,t_id);
     if(!SM_IS_NTH_T_NEW(sm,t_id))
	{
	  smNew_tri_cnt++;
	  SM_SET_NTH_T_NEW(sm,t_id);
	} 
     t_id = smTri_next_ccw_nbr(sm,t,b_id);
    }
}

int
smReplace_samp(sm,c,dir,p,s_id,t_id,o_id,on,which)
     SM *sm;
     COLR c;
     FVECT dir,p;
     int s_id,t_id,o_id,on,which;
{
  int v_id,tri_id;
  TRI *t,*tri;

  tri = SM_NTH_TRI(sm,t_id);
  v_id = T_NTH_V(tri,which);

  /* If it is a base id, need new sample */
  if(SM_BASE_ID(sm,v_id))
  {
    sInit_samp(SM_SAMP(sm),s_id,c,dir,p,o_id);
    smReplace_base_samp(sm,v_id,s_id,tri,t_id,which);
    return(s_id);
  }
  if(dir)
  {
    /* If world point */
    /* if existing point is a dir: leave */
    if(SM_DIR_ID(sm,v_id))
      return(INVALID);
    if(on == ON_V)
    {
      sInit_samp(SM_SAMP(sm),v_id,c,dir,p,o_id);

     if(!SM_IS_NTH_T_NEW(sm,t_id))
	{
	  smNew_tri_cnt++;
	  SM_SET_NTH_T_NEW(sm,t_id);
	} 
      tri_id = smTri_next_ccw_nbr(sm,tri,v_id);
      while(tri_id != t_id)
      {
	t = SM_NTH_TRI(sm,tri_id);
     if(!SM_IS_NTH_T_NEW(sm,tri_id))
	{
	  smNew_tri_cnt++;
	  SM_SET_NTH_T_NEW(sm,tri_id);
	} 

	tri_id = smTri_next_ccw_nbr(sm,t,v_id);
      }
      return(v_id);
    }
    /* on == ON_P */
    else
   {
     FVECT spt,npt;
     double d,d2;

    /* Need to check which sample is preferable */
     VSUB(spt,p,SM_VIEW_CENTER(sm));
     normalize(spt);
	
     VSUB(npt,SM_NTH_WV(sm,v_id),SM_VIEW_CENTER(sm));
     normalize(npt);
     d = fdir2diff(SM_NTH_W_DIR(sm,v_id), npt);
     d2 = 2. - 2.*DOT(dir,spt);
      /* The existing sample is a better sample:punt */
     if(d2 < d)
       {
	 /* The new sample has better information- replace values */
	 sInit_samp(SM_SAMP(sm),v_id,c,dir,p,o_id);
	 if(!SM_IS_NTH_T_NEW(sm,t_id))
	    {
	      smNew_tri_cnt++;
	      SM_SET_NTH_T_NEW(sm,t_id);
	    } 

	 tri_id = smTri_next_ccw_nbr(sm,tri,v_id);
	 while(tri_id != t_id)
	   {
	     t = SM_NTH_TRI(sm,tri_id);
	     if(!SM_IS_NTH_T_NEW(sm,tri_id))
	    {
	      smNew_tri_cnt++;
	      SM_SET_NTH_T_NEW(sm,tri_id);
	    } 
	     tri_id = smTri_next_ccw_nbr(sm,t,v_id);
	   }
       }
    return(v_id);
   }
  }
  else
    { /* New point is a dir */
      return(INVALID);
    }
}

int
smAlloc_samp(sm)
SM *sm;
{
  int s_id,replaced,cnt;
  SAMP *s;
  FVECT p;

  s = SM_SAMP(sm);
  s_id = sAlloc_samp(s,&replaced);

  cnt=0;
  while(replaced)
  {
    if(smRemoveVertex(sm,s_id))
      break;
    s_id = sAlloc_samp(s,&replaced);
    cnt++;
    if(cnt > S_MAX_SAMP(s))
      error(CONSISTENCY,"smAlloc_samp():unable to find free samp\n");
  }
  return(s_id);
}


/* Add sample to the mesh:

   the sample can be a world space or directional point. If o_id !=INVALID,
   then we have an existing sample containing the appropriate information
   already converted into storage format.
   The spherical projection of the point/ray can:
        a) coincide with existing vertex
	        i) conincides with existing sample
	       ii) projects same as existing sample
	      iii) hits a base vertex
	b) coincide with existing edge
	c) Fall in existing triangle
*/
int
smAdd_samp(sm,c,dir,p,o_id)
   SM *sm;
   COLR c;
   FVECT dir,p;
   int o_id;
{
  int t_id,s_id,r_id,on,which,n_id;
  double d;
  FVECT wpt;

  r_id = INVALID;

  /* Must do this first-as may change mesh */
  s_id = smAlloc_samp(sm);
  /* If sample is a world space point */
  if(p)
  {
    while(1)
    {
      t_id = smPointLocateTri(sm,p,&on,&which);
      if(t_id == INVALID)
      {
#ifdef DEBUG
	  eputs("smAddSamp(): unable to locate tri containing sample \n");
#endif
	  smUnalloc_samp(sm,s_id);
	  return(INVALID);
	}
      /* If spherical projection coincides with existing sample: replace */
      if((on == ON_V || on == ON_P))
      {
	  n_id = smReplace_samp(sm,c,dir,p,s_id,t_id,o_id,on,which);
	  if(n_id!= s_id)
	     smUnalloc_samp(sm,s_id);
	  return(n_id);
      }
      if((!(smTest_sample(sm,t_id,dir,p,&r_id))))
     { 
	smUnalloc_samp(sm,s_id);
	return(INVALID);
      }
      if(r_id != INVALID)
      {
	smRemoveVertex(sm,r_id);
	sDelete_samp(SM_SAMP(sm),r_id);
      }
      else
	break;
    }
    /* If sample is being added in the middle of the sample array: tone 
       map individually
       */
    /* Initialize sample */
    sInit_samp(SM_SAMP(sm),s_id,c,dir,p,o_id);
    
  }
    /* If sample is a direction vector */
    else
    {
      VADD(wpt,dir,SM_VIEW_CENTER(sm));
      while(1)
      	{ 
	    t_id = smPointLocateTri(sm,wpt,&on,&which);
	    if(t_id == INVALID)
	       {
#ifdef DEBUG
		   eputs("smAddSamp(): can'tlocate tri containing samp\n");
#endif
		   smUnalloc_samp(sm,s_id);
		   return(INVALID);
	       }
	    if(on == ON_V || on == ON_P)
            {
		n_id=smReplace_samp(sm,c,NULL,wpt,s_id,t_id,o_id,on,which);
		if(n_id !=s_id)
		   smUnalloc_samp(sm,s_id);
		return(n_id);
	    }
	    if((!(smTest_sample(sm,t_id,NULL,wpt,&r_id))))
	    {
		smUnalloc_samp(sm,s_id);
		return(INVALID);
	    }
	    if(r_id != INVALID)
            {
		smRemoveVertex(sm,r_id);
		sDelete_samp(SM_SAMP(sm),r_id);
	    }
	    else
	       break;
	}
      /* Allocate space for a sample and initialize */
      sInit_samp(SM_SAMP(sm),s_id,c,NULL,wpt,o_id);
    }
  if(!SM_DIR_ID(sm,s_id))
    {
      /* If not a base or sky point, add distance from the 
	 viewcenter to average*/
      d = DIST(SM_NTH_WV(sm,s_id),SM_VIEW_CENTER(sm));
      smDist_sum += 1.0/d;
    }
    smInsert_samp(sm,s_id,t_id,on,which);

    return(s_id);
}

/*
 * int
 * smNewSamp(c, dir, p)	: register new sample point and return index
 * COLR	c;		: pixel color (RGBE)
 * FVECT	dir;	: ray direction vector
 * FVECT	p;	: world intersection point
 *
 * Add new sample point to data structures, removing old values as necessary.
 * New sample representation will be output in next call to smUpdate().
 * If the point is a sky point: then v=NULL
 */
int
smNewSamp(c,dir,p)
COLR c;
FVECT dir;
FVECT p;
{
    int s_id;

    /* First check if this the first sample: if so initialize mesh */
    if(SM_NUM_SAMP(smMesh) == 0)
    {
      smInit_sm(smMesh,odev.v.vp);
      sClear_all_flags(SM_SAMP(smMesh));
    }
    /* Add the sample to the mesh */
    s_id = smAdd_samp(smMesh,c,dir,p,INVALID);

    return(s_id);
    
 }    
int
smAdd_base_vertex(sm,v)
   SM *sm;
   FVECT v;
{
  int id;

  /* First add coordinate to the sample array */
  id = sAdd_base_point(SM_SAMP(sm),v);
  if(id == INVALID)
    return(INVALID);
  /* Initialize triangle pointer to -1 */
  smClear_vert(sm,id);
  return(id);
}



/* Initialize a the point location DAG based on a 6 triangle tesselation
   of the unit sphere centered on the view center. The DAG structure
   contains 6 roots: one for each initial base triangle
*/

int
smCreate_base_mesh(sm,type)
SM *sm;
int type;
{
  int i,s_id,tri_id,nbr_id;
  int p[SM_EXTRA_POINTS],ids[SM_BASE_TRIS];
  int v0_id,v1_id,v2_id;
  FVECT d,pt,cntr,v0,v1,v2;
  TRI *t;

  /* First insert the base vertices into the sample point array */

  for(i=0; i < SM_EXTRA_POINTS; i++)
  {
    VADD(cntr,icosa_verts[i],SM_VIEW_CENTER(sm));
    p[i]  = smAdd_base_vertex(sm,cntr);
  }
  /* Create the base triangles */
  for(i=0;i < SM_BASE_TRIS; i++)
  {
    v0_id = p[icosa_tris[i][0]];
    v1_id = p[icosa_tris[i][1]];
    v2_id = p[icosa_tris[i][2]];
    ids[i] = smAdd_tri(sm, v0_id,v1_id,v2_id);
    /* Set neighbors */
    for(nbr_id=0; nbr_id < 3; nbr_id++)
      T_NTH_NBR(SM_NTH_TRI(sm,ids[i]),nbr_id) = icosa_nbrs[i][nbr_id];

  }
  for(i=0;i < SM_BASE_TRIS; i++)
  {
    t = SM_NTH_TRI(sm,ids[i]);
    smLocator_add_tri(sm,ids[i],T_NTH_V(t,0),T_NTH_V(t,1),T_NTH_V(t,2));
  }
 return(1);

}


int
smNext_tri_flag_set(sm,i,which,b)
     SM *sm;
     int i,which;
     int b;
{

  for(; i < SM_NUM_TRI(sm);i++)
  {

    if(!T_IS_VALID(SM_NTH_TRI(sm,i)))
	 continue;
    if(!SM_IS_NTH_T_FLAG(sm,i,which))
	continue;
    if(!b)
      break;
    if((b==1) && !SM_BG_TRI(sm,i))
      break;
    if((b==2) && SM_BG_TRI(sm,i))
      break;
  }
     
  return(i);
}



int
smNext_valid_tri(sm,i)
     SM *sm;
     int i;
{

  while( i < SM_NUM_TRI(sm) && !T_IS_VALID(SM_NTH_TRI(sm,i)))
     i++;
     
  return(i);
}



qtTri_from_id(t_id,v0,v1,v2)
int t_id;
FVECT v0,v1,v2;
{
  TRI *t;
  
  t = SM_NTH_TRI(smMesh,t_id);
  if(!T_IS_VALID(t))
    return(0);
  VSUB(v0,SM_T_NTH_WV(smMesh,t,0),SM_VIEW_CENTER(smMesh));
  VSUB(v1,SM_T_NTH_WV(smMesh,t,1),SM_VIEW_CENTER(smMesh));
  VSUB(v2,SM_T_NTH_WV(smMesh,t,2),SM_VIEW_CENTER(smMesh));
  return(1);
}


smRebuild_mesh(sm,v)
   SM *sm;
   VIEW *v;
{
    int i,j,cnt;
    FVECT p,ov,dir;
    double d;

#ifdef DEBUG
    fprintf(stderr,"smRebuild_mesh(): rebuilding....");
#endif
    /* Clear the mesh- and rebuild using the current sample array */
    /* Remember the current number of samples */
    cnt = SM_NUM_SAMP(sm);
    /* Calculate the difference between the current and new viewpoint*/
    /* Will need to subtract this off of sky points */
    VCOPY(ov,SM_VIEW_CENTER(sm));
    /* Initialize the mesh to 0 samples and the base triangles */

    /* Go through all samples and add in if in the new view frustum, and
       the dir is <= 30 degrees from new view
     */
    smInit_sm(sm,v->vp);
    for(i=0; i < cnt; i++)
    {
      /* First check if sample visible(conservative approx: if one of tris 
	 attached to sample is in frustum)	 */
      if(!S_IS_FLAG(i))
	continue;

      /* Next test if direction > 30 from current direction */
	if(SM_NTH_W_DIR(sm,i)!=-1)
	{
	    VSUB(p,SM_NTH_WV(sm,i),v->vp);
	    normalize(p);
	    decodedir(dir,SM_NTH_W_DIR(sm,i));
	    d = 2. - 2.*DOT(dir,p);
	    if (d > MAXDIFF2)
	      continue;
	    VCOPY(p,SM_NTH_WV(sm,i));
	    smAdd_samp(sm,NULL,dir,p,i);
	}
	else
	{
	  VSUB(dir,SM_NTH_WV(sm,i),ov);
	  VADD(SM_NTH_WV(sm,i),dir,SM_VIEW_CENTER(sm));
	  smAdd_samp(sm,NULL,dir,NULL,i);
	}

    }
#ifdef DEBUG
    fprintf(stderr,"smRebuild_mesh():done\n");
#endif
}

int
intersect_tri_set(t_set,orig,dir,pt)
   OBJECT *t_set;
   FVECT orig,dir,pt;
{
    OBJECT *optr;
    int i,t_id,id,base;
    int pid0,pid1,pid2;
    FVECT p0,p1,p2,p;
    TRI *t;
    double d,d1;
    
    optr = QT_SET_PTR(t_set);
    for(i = QT_SET_CNT(t_set); i > 0; i--)
    {
	t_id = QT_SET_NEXT_ELEM(optr);
	t = SM_NTH_TRI(smMesh,t_id);
	if(!T_IS_VALID(t))
	  continue;
	pid0 = T_NTH_V(t,0);
	pid1 = T_NTH_V(t,1);
	pid2 = T_NTH_V(t,2);
	if(base = SM_IS_NTH_T_BASE(smMesh,t_id))
	  if(SM_BASE_ID(smMesh,pid0) && SM_BASE_ID(smMesh,pid1) &&
	     SM_BASE_ID(smMesh,pid2))
	    continue;
	VCOPY(p0,SM_NTH_WV(smMesh,pid0));
	VCOPY(p1,SM_NTH_WV(smMesh,pid1));
	VCOPY(p2,SM_NTH_WV(smMesh,pid2));
	if(ray_intersect_tri(orig,dir,p0,p1,p2,p))
        {
	  if(!base)
	  {
	    d =  DIST_SQ(p,p0);
	    d1 = DIST_SQ(p,p1);
	    if(d < d1)
	      {
		d1 = DIST_SQ(p,p2);
		id = (d1 < d)?pid2:pid0;
	      }
	    else
	      {
		d = DIST_SQ(p,p2);
		id = (d < d1)? pid2:pid1;
	      }
	  }
	  else
	    {
	      if(SM_BASE_ID(smMesh,pid0))
	      {
		if(SM_BASE_ID(smMesh,pid1))
		  id = pid2;
		else
		  if(SM_BASE_ID(smMesh,pid2))
		    id = pid1;
		  else
		  {
		    d =  DIST_SQ(p,p1);
		    d1 = DIST_SQ(p,p2);
		    if(d < d1)
		      id = pid1;
		    else
		      id = pid2;
		  }
	      }
	      else
	      {
		if(SM_BASE_ID(smMesh,pid1))
		{
		  if(SM_BASE_ID(smMesh,pid2))
		    id = pid0;
		  else
		  {
		    d =  DIST_SQ(p,p0);
		    d1 = DIST_SQ(p,p2);
		    if(d < d1)
		      id = pid0;
		    else
		      id = pid2;
		  }
		}
		else
		{
		  d =  DIST_SQ(p,p0);
		  d1 = DIST_SQ(p,p1);
		  if(d < d1)
		    id = pid0;
		  else
		    id = pid1;
		}
	      }
	    }
	  if(pt)
	    VCOPY(pt,p);
#ifdef TEST_DRIVER
	  Pick_tri = t_id;
	  Pick_samp = id;
	  VCOPY(Pick_point[0],p);
#endif
	  return(id);
	}
    }
    return(-1);
}

/* OS is constrained to be <= QT_MAXCSET : if the set exceeds this, the
 results of check_set are conservative
*/
int
compare_ids(id1,id2)
OBJECT *id1,*id2;
{
  int d;

  d = *id2-*id1;
  
  if(d > 0)
    return(-1);
  if(d < 0)
    return(1);
  
  return(0);
}

ray_trace_check_set(qt,argptr,fptr)
   QUADTREE qt;
   RT_ARGS *argptr;
   int *fptr;
{
    OBJECT tset[QT_MAXSET+1],*tptr;	
    double dt,t;
    int found;
  if(QT_IS_EMPTY(qt)) 
    return;
  if(QT_LEAF_IS_FLAG(qt)) 
  { 
    QT_FLAG_SET_DONE(*fptr); 
#if DEBUG 
       eputs("ray_trace_check_set():Already visited this node:aborting\n"); 
#endif 
       return;
  } 
  else
    QT_LEAF_SET_FLAG(qt);

  tptr = qtqueryset(qt); 
  if(QT_SET_CNT(tptr) > QT_MAXSET) 
    tptr = (OBJECT *)malloc((QT_SET_CNT(tptr)+1)*sizeof(OBJECT)); 
  else 
    tptr = tset; 
  if(!tptr) 
    goto memerr; 
    
  qtgetset(tptr,qt); 
  /* Must sort */ 
  qsort((void *)(&(tptr[1])),tptr[0],sizeof(OBJECT),compare_ids); 
  /* Check triangles in set against those seen so far(os):only 
     check new triangles for intersection (t_set')  
     */ 
  check_set_large(tptr,argptr->os); 

  if(!QT_SET_CNT(tptr))
    return;
  found = intersect_tri_set(tptr,argptr->orig,argptr->dir,NULL);
  if(tptr != tset) 
    free(tptr); 
  if(found != INVALID)
  { 
    argptr->t_id = found; 
    QT_FLAG_SET_DONE(*fptr); 
  } 
  return;
memerr:
    error(SYSTEM,"ray_trace_check_set():Unable to allocate memory");
}


/*
 * int
 * smFindSamp(FVECT orig, FVECT dir) 
 *
 * Find the closest sample to the given ray.  Returns sample id, -1 on failure.
 * "dir" is assumed to be normalized
 */

int
smFindSamp(orig,dir)
FVECT orig,dir;
{
  FVECT b,p,o;
  OBJECT *ts;
  QUADTREE qt;
  int s_id,test;
  double d;

 /*  r is the normalized vector from the view center to the current
  *  ray point ( starting with "orig"). Find the cell that r falls in,
  *  and test the ray against all triangles stored in the cell. If
  *  the test fails, trace the projection of the ray across to the
  *  next cell it intersects: iterate until either an intersection
  *  is found, or the projection ray is // to the direction. The sample
  *  corresponding to the triangle vertex closest to the intersection
  *  point is returned.
  */
  
  /* First test if "orig" coincides with the View_center or if "dir" is
     parallel to r formed by projecting "orig" on the sphere. In
     either case, do a single test against the cell containing the
     intersection of "dir" and the sphere
   */
  /* orig will be updated-so preserve original value */
  if(!smMesh)
     return;
#ifdef TEST_DRIVER
  Picking= TRUE;
#endif  

  if(EQUAL_VEC3(orig,SM_VIEW_CENTER(smMesh)))
  {
    qt = smPointLocateCell(smMesh,dir);
    if(QT_IS_EMPTY(qt))
      goto Lerror;
    ts = qtqueryset(qt);
    s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
    VCOPY(Pick_point[0],p);
#endif
#ifdef DEBUG
    fprintf(stderr,"Simple pick returning %d\n",s_id);
#endif

    return(s_id);
  }
  d = point_on_sphere(b,orig,SM_VIEW_CENTER(smMesh));
  if(EQUAL_VEC3(b,dir))
  {
    qt = smPointLocateCell(smMesh,dir);
    if(QT_IS_EMPTY(qt))
      goto Lerror;
    ts = qtqueryset(qt);
    s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
	VCOPY(Pick_point[0],p);
#endif
#ifdef DEBUG
    fprintf(stderr,"Simple pick2 returning %d\n",s_id);
#endif
	return(s_id);
  }
  if(OPP_EQUAL_VEC3(b,dir))
  {
    qt = smPointLocateCell(smMesh,orig);
    if(QT_IS_EMPTY(qt))
      goto Lerror;
    ts = qtqueryset(qt);
    s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
	VCOPY(Pick_point[0],p);
#endif
	if(s_id != INVALID)
	{
#ifdef DEBUG
    fprintf(stderr,"Front pick returning %d\n",s_id);
#endif
	  return(s_id);
	}
	qt = smPointLocateCell(smMesh,dir);
	if(QT_IS_EMPTY(qt))
	  goto Lerror;
	ts = qtqueryset(qt);
	s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
	VCOPY(Pick_point[0],p);
#endif
#ifdef DEBUG
    fprintf(stderr,"Back pick returning %d\n",s_id);
#endif
	return(s_id);
  }
  {
    OBJECT t_set[QT_MAXCSET + 1];
    RT_ARGS rt;
    FUNC func;

    /* Test each of the root triangles against point id */
    QT_CLEAR_SET(t_set);
    VSUB(o,orig,SM_VIEW_CENTER(smMesh));
    ST_CLEAR_FLAGS(SM_LOCATOR(smMesh));
    rt.t_id = -1;
    rt.os = t_set;
    VCOPY(rt.orig,orig);
    VCOPY(rt.dir,dir);
    F_FUNC(func) = ray_trace_check_set;
    F_ARGS(func) = (int *)(&rt);
    stTrace_ray(SM_LOCATOR(smMesh),o,dir,func);
    s_id = rt.t_id;
  }    
#ifdef DEBUG
    fprintf(stderr,"Trace pick returning %d\n",s_id);
#endif
  return(s_id);

Lerror:
#ifdef DEBUG
  eputs("smFindSamp(): point not found");
#endif  
  return(INVALID);

}


mark_active_tris(argptr,root,qt)
     int *argptr;
     int root;
     QUADTREE qt;
{
  OBJECT *os,*optr;
  register int i,t_id;
  TRI *tri;

  if(QT_IS_EMPTY(qt) || QT_LEAF_IS_FLAG(qt))
    return;
  
  /* For each triangle in the set, set the which flag*/
  os = qtqueryset(qt);

  for (i = QT_SET_CNT(os), optr = QT_SET_PTR(os); i > 0; i--)
  {
    t_id = QT_SET_NEXT_ELEM(optr);
    /* Set the render flag */
     tri = SM_NTH_TRI(smMesh,t_id);
     if(!T_IS_VALID(tri))
	    continue;
     SM_SET_NTH_T_ACTIVE(smMesh,t_id);
     /* Set the Active bits of the Vertices */
     S_SET_FLAG(T_NTH_V(tri,0));
     S_SET_FLAG(T_NTH_V(tri,1));
     S_SET_FLAG(T_NTH_V(tri,2));
  }
 }


mark_tris_in_frustum(view)
 VIEW *view;
 {
     FVECT nr[4],far[4];
     FPEQ peq;
     int debug=0;
     FUNC f;

     F_FUNC(f) = mark_active_tris;
     F_ARGS(f) = NULL;

     /* Mark triangles in approx. view frustum as being active:set
	LRU counter: for use in discarding samples when out
	of space
	Radiance often has no far clipping plane: but driver will set
	dev_zmin,dev_zmax to satisfy OGL
     */

     /* First clear all the quadtree node and triangle active flags */
     qtClearAllFlags();
     smClear_flags(smMesh,T_ACTIVE_FLAG);
     /* Clear all of the active sample flags*/
     sClear_all_flags(SM_SAMP(smMesh));


     /* calculate the world space coordinates of the view frustum */
     calculate_view_frustum(view->vp,view->hvec,view->vvec,view->horiz,
			    view->vert, dev_zmin,dev_zmax,nr,far);

#ifdef TEST_DRIVER
     VCOPY(FrustumFar[0],far[0]);
     VCOPY(FrustumFar[1],far[1]);
     VCOPY(FrustumFar[2],far[2]);
     VCOPY(FrustumFar[3],far[3]);
     VCOPY(FrustumNear[0],nr[0]);
     VCOPY(FrustumNear[1],nr[1]);
     VCOPY(FrustumNear[2],nr[2]);
     VCOPY(FrustumNear[3],nr[3]);
#endif
     /* Project the view frustum onto the spherical quadtree */
     /* For every cell intersected by the projection of the faces

	of the frustum: mark all triangles in the cell as ACTIVE-
	Also set the triangles LRU clock counter
	*/

     if(EQUAL_VEC3(view->vp,SM_VIEW_CENTER(smMesh)))
     {/* Near face triangles */

       smLocator_apply(smMesh,nr[0],nr[2],nr[3],f);
       smLocator_apply(smMesh,nr[2],nr[0],nr[1],f); 
       return;
     }
     /* Test the view against the planes: and swap orientation if inside:*/
     tri_plane_equation(nr[0],nr[2],nr[3], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     {/* Near face triangles */
       smLocator_apply(smMesh,nr[3],nr[2],nr[0],f);
       smLocator_apply(smMesh,nr[1],nr[0],nr[2],f);
     }
     else
     {/* Near face triangles */
       smLocator_apply(smMesh,nr[0],nr[2],nr[3],f);
       smLocator_apply(smMesh,nr[2],nr[0],nr[1],f);
     }
     tri_plane_equation(nr[0],far[3],far[0], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     { /* Right face triangles */
       smLocator_apply(smMesh,far[0],far[3],nr[0],f);
       smLocator_apply(smMesh,nr[3],nr[0],far[3],f);
     }
     else
     {/* Right face triangles */
       smLocator_apply(smMesh,nr[0],far[3],far[0],f);
       smLocator_apply(smMesh,far[3],nr[0],nr[3],f);
     }

     tri_plane_equation(nr[1],far[2],nr[2], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     { /* Left face triangles */
       smLocator_apply(smMesh,nr[2],far[2],nr[1],f);
       smLocator_apply(smMesh,far[1],nr[1],far[2],f);
     }
     else
     { /* Left face triangles */
       smLocator_apply(smMesh,nr[1],far[2],nr[2],f);
       smLocator_apply(smMesh,far[2],nr[1],far[1],f);
     }
     tri_plane_equation(nr[0],far[0],nr[1], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     {/* Top face triangles */
       smLocator_apply(smMesh,nr[1],far[0],nr[0],f);
       smLocator_apply(smMesh,far[1],far[0],nr[1],f);
     }
     else
     {/* Top face triangles */
       smLocator_apply(smMesh,nr[0],far[0],nr[1],f);
       smLocator_apply(smMesh,nr[1],far[0],far[1],f);
     }
     tri_plane_equation(nr[3],nr[2],far[3], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     {/* Bottom face triangles */
       smLocator_apply(smMesh,far[3],nr[2],nr[3],f);
       smLocator_apply(smMesh,far[3],far[2],nr[2],f);
     }
     else
     { /* Bottom face triangles */
       smLocator_apply(smMesh,nr[3],nr[2],far[3],f);
       smLocator_apply(smMesh,nr[2],far[2],far[3],f);
     }
      tri_plane_equation(far[2],far[0],far[1], &peq,FALSE);
     if(PT_ON_PLANE(SM_VIEW_CENTER(smMesh),peq) < 0.0)
     {/* Far face triangles */
       smLocator_apply(smMesh,far[0],far[2],far[1],f);
       smLocator_apply(smMesh,far[2],far[0],far[3],f);
     }
     else
     {/* Far face triangles */
       smLocator_apply(smMesh,far[1],far[2],far[0],f);
       smLocator_apply(smMesh,far[3],far[0],far[2],f);
     }

 }










