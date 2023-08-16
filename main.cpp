#include <mod/amlmod.h>
#include <mod/logger.h>
#include <time.h>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
    #define BYVER(__for32, __for64) (__for32)
#else
    #include "GTASA_STRUCTS_210.h"
    #define BYVER(__for32, __for64) (__for64)
#endif
#define sizeofA(__aVar)  ((int)(sizeof(__aVar)/sizeof(__aVar[0])))

MYMOD(net.re3.gennariarmando.rusjj.rubbish, Rubbish, 1.1, re3 Team & gennariarmando & RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0.2.2)
END_DEPLIST()

uintptr_t pGTASA;
void* hGTASA;

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Variables
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
const float aAnimations[3][34] = {
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },

    { 0.0f, 0.05f, 0.12f, 0.25f, 0.42f, 0.57f, 0.68f, 0.8f, 0.86f, 0.9f, 0.93f, 0.95f, 0.96f, 0.97f, 0.98f, 0.99f, 1.0f,
      0.15f, 0.35f, 0.6f, 0.9f, 1.2f, 1.25f, 1.3f, 1.2f, 1.1f, 0.95f, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f, 0.1f, 0 },

    { 0.0f, 0.05f, 0.12f, 0.25f, 0.42f, 0.57f, 0.68f, 0.8f, 0.95f, 1.1f, 1.15f, 1.18f, 1.15f, 1.1f, 1.05f, 1.03f, 1.0f,
      0.15f, 0.35f, 0.6f, 0.9f, 1.2f, 1.25f, 1.3f, 1.2f, 1.1f, 0.95f, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f, 0.1f, 0 }
};
TextureDatabaseRuntime* txdRubbish;
RwTexture* gpRubbishTexture[6];
uint16_t RubbishIndexList[6];
RwIm3DVertex RubbishVertices[4];

unsigned int *currArea, *m_snTimeInMilliseconds, *m_FrameCounter;
int *TempBufferVerticesStored, *TempBufferIndicesStored;
VertexBuffer *TempVertexBuffer;
uint16_t *TempBufferRenderIndexList;
CCamera *TheCamera;
float *ms_fTimeStep, *Wind, *m_fDNBalanceParam;
CBulletTrace *aTraces;

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Functions
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
void (*RwRenderStateSet)(RwRenderState, void*);
void* (*RwIm3DTransform)(RwIm3DVertex *pVerts, RwUInt32 numVerts, RwMatrix *ltm, RwUInt32 flags);
bool (*RwIm3DRenderIndexedPrimitive)(RwPrimitiveType primType, uint16_t *indices, RwInt32 numIndices);
bool (*RwIm3DEnd)();
float (*FindGroundZFor3DCoord)(float,float,float,bool *,CEntity **);
int (*FindAttributesForCoors)(CVector);
TextureDatabaseRuntime* (*LoadTextureDatabase)(char const*,bool,TextureDatabaseFormat);
int (*GetEntry)(TextureDatabaseRuntime *,char const*, bool*);
RwTexture* (*GetRWTexture)(TextureDatabaseRuntime *, int);
bool (*ProcessVerticalLine)(CVector const&,float,CColPoint &,CEntity *&,bool,bool,bool,bool,bool,bool,CStoredCollPoly *);
CColModel* (*GetColModel)(CEntity*);
void (*RwTextureDestroy)(RwTexture*);
bool (*GetWaterLevelNoWaves)(float x, float y, float z, float *pWaterZ, float *pBigWaves, float *pSmallWaves);

inline RwTexture* GetTextureFromTexDB(TextureDatabaseRuntime* texdb, const char* name)
{
    bool hasSiblings;
    return GetRWTexture(texdb, GetEntry(texdb, name, &hasSiblings));
}
inline float sqr(float v) { return v*v; }
inline float RandomFloat(float min, float max)
{
    float r = (float)rand() / (float)RAND_MAX;
    return min + r * (max - min);
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Rubbish Class
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
class COneSheet
{
public:
    void AddToList(COneSheet* list)
    {
        this->m_next = list->m_next;
        this->m_prev = list;
        list->m_next = this;
        this->m_next->m_prev = this;
    }
    void RemoveFromList()
    {
        m_next->m_prev = m_prev;
        m_prev->m_next = m_next;
    }

public:
    COneSheet* m_prev;
    COneSheet* m_next;
    CVector m_basePos;
    CVector m_animatedPos;
    float m_targetZ;
    float m_scale;
    uint32_t m_moveStart;
    uint32_t m_moveDuration;
    float m_animHeight;
    float m_xDist;
    float m_yDist;
    float m_angle;
    int8_t m_state;
    int8_t m_animationType;
    bool m_isVisible;
    bool m_targetIsVisible;
};
class CRubbish
{
public:
    static void Init()
    {
        for (int32_t i = 0; i < NUM_RUBBISH_SHEETS; ++i)
        {
            aSheets[i].m_state = 0;
            if (i < NUM_RUBBISH_SHEETS - 1) aSheets[i].m_next = &aSheets[i + 1];
            else aSheets[i].m_next = &EndEmptyList;

            if (i > 0) aSheets[i].m_prev = &aSheets[i - 1];
            else aSheets[i].m_prev = &StartEmptyList;
        }

        StartEmptyList.m_next = &aSheets[0];
        StartEmptyList.m_prev = NULL;
        EndEmptyList.m_next = NULL;
        EndEmptyList.m_prev = &aSheets[NUM_RUBBISH_SHEETS - 1];

        StartStaticsList.m_next = &EndStaticsList;
        StartStaticsList.m_prev = NULL;
        EndStaticsList.m_next = NULL;
        EndStaticsList.m_prev = &StartStaticsList;

        StartMoversList.m_next = &EndMoversList;
        StartMoversList.m_prev = NULL;
        EndMoversList.m_next = NULL;
        EndMoversList.m_prev = &StartMoversList;

        RubbishVertices[0].texCoords.u = 0.0f; RubbishVertices[0].texCoords.v = 0.0f;
        RubbishVertices[1].texCoords.u = 1.0f; RubbishVertices[1].texCoords.v = 0.0f;
        RubbishVertices[2].texCoords.u = 0.0f; RubbishVertices[2].texCoords.v = 1.0f;
        RubbishVertices[3].texCoords.u = 1.0f; RubbishVertices[3].texCoords.v = 1.0f;

        RubbishIndexList[0] = 0;
        RubbishIndexList[1] = 1;
        RubbishIndexList[2] = 2;
        RubbishIndexList[3] = 1;
        RubbishIndexList[4] = 3;
        RubbishIndexList[5] = 2;

        m_bVisible = true;
        m_nVisibility = 255;

        RUBBISH_MAX_DIST = 18.0f * RUBBISH_DIST_MULTIPLIER;
        RUBBISH_FADE_DIST = 16.5f * RUBBISH_DIST_MULTIPLIER;

        txdRubbish = LoadTextureDatabase("rubbish", true, DF_Default);
        if(txdRubbish)
        {
            gpRubbishTexture[0] = GetTextureFromTexDB(txdRubbish, "palm_leaf01_64");
            gpRubbishTexture[1] = GetTextureFromTexDB(txdRubbish, "palm_leaf02_64");
            gpRubbishTexture[2] = GetTextureFromTexDB(txdRubbish, "generic_leaf01_256");
            gpRubbishTexture[3] = GetTextureFromTexDB(txdRubbish, "generic_leaf02_256");
            gpRubbishTexture[4] = GetTextureFromTexDB(txdRubbish, "newspaper01_256");
            gpRubbishTexture[5] = GetTextureFromTexDB(txdRubbish, "newspaper02_256");
        }
    }
    static void Shutdown()
    {
        RwTextureDestroy(gpRubbishTexture[0]);
        RwTextureDestroy(gpRubbishTexture[1]);
        RwTextureDestroy(gpRubbishTexture[2]);
        RwTextureDestroy(gpRubbishTexture[3]);
        RwTextureDestroy(gpRubbishTexture[4]);
        RwTextureDestroy(gpRubbishTexture[5]);

        gpRubbishTexture[0] = NULL;
        gpRubbishTexture[1] = NULL;
        gpRubbishTexture[2] = NULL;
        gpRubbishTexture[3] = NULL;
        gpRubbishTexture[4] = NULL;
        gpRubbishTexture[5] = NULL;
    }
    static void Render()
    {
        if(*currArea != 0 || !m_nVisibility) return;

        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)false);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)true);
        RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)true);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)true);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

        for(int type = 0; type < 6; ++type)
        {
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, gpRubbishTexture[type]->raster);

            *TempBufferVerticesStored = 0;
            *TempBufferIndicesStored = 0;

            COneSheet *sheet, *endsheet = &aSheets[(type + 1) * NUM_RUBBISH_SHEETS_PART];
            for (sheet = &aSheets[type * NUM_RUBBISH_SHEETS_PART]; sheet < endsheet; ++sheet)
            {
                if(sheet->m_state == 0) continue;
                int alpha = DEFAULT_RUBBISH_ALPHA;

                CVector pos;
                if (sheet->m_state == 1)
                {
                    pos = sheet->m_basePos;
                    if (!sheet->m_isVisible) alpha = 0;
                }
                else
                {
                    pos = sheet->m_animatedPos;
                    if (!sheet->m_isVisible || !sheet->m_targetIsVisible)
                    {
                        float t = (float)(*m_snTimeInMilliseconds - sheet->m_moveStart) / sheet->m_moveDuration;
                        float f1 = sheet->m_isVisible ? 1.0f - t : 0.0f;
                        float f2 = sheet->m_targetIsVisible ? t : 0.0f;
                        alpha = DEFAULT_RUBBISH_ALPHA * (f1 + f2);
                    }
                }

                float camDist = (pos - TheCamera->GetPosition()).Magnitude2D();
                if (camDist < RUBBISH_MAX_DIST)
                {
                    if (camDist >= RUBBISH_FADE_DIST)
                    {
                        alpha -= alpha * (camDist - RUBBISH_FADE_DIST) / (RUBBISH_MAX_DIST - RUBBISH_FADE_DIST);
                    }
                    alpha = (m_nVisibility * alpha) / 256;

                    float vsin = sinf(sheet->m_angle), vcos = cosf(sheet->m_angle);
                    float vx1, vy1, vx2, vy2;

                    if (type < 2) // Palm Leaves
                    {
                        vx1 =  0.3f * vsin * sheet->m_scale;
                        vy1 =  0.3f * vcos * sheet->m_scale;
                        vx2 =  0.9f * vcos * sheet->m_scale;
                        vy2 = -0.9f * vsin * sheet->m_scale;
                    }
                    else if (type < 4) // Leaves
                    {
                        vx1 =  0.2f * vsin * sheet->m_scale;
                        vy1 =  0.2f * vcos * sheet->m_scale;
                        vx2 =  0.2f * vcos * sheet->m_scale;
                        vy2 = -0.2f * vsin * sheet->m_scale;
                    }
                    else // Newspaper
                    {
                        vx1 =  0.4f * vsin;
                        vy1 =  0.4f * vcos;
                        vx2 =  0.4f * vcos;
                        vy2 = -0.4f * vsin;
                    }

                    int brightness = 220;
                    //CEntity *outEntity; CColPoint outColPoint;
                    //if (ProcessVerticalLine(pos, -100.0, outColPoint, outEntity, 1, 0, 0, 0, 0, 0, NULL))
                    //{
                    //    brightness = 128 + (uint8_t)(127 *
                    //            outColPoint.m_nLightingB.day
                    //            * 0.033333333f
                    //            * (1.0f - *m_fDNBalanceParam)
                    //            + outColPoint.m_nLightingB.night
                    //            * 0.033333333f
                    //            * *m_fDNBalanceParam);
                    //}

                    int v = *TempBufferVerticesStored;
                    TempVertexBuffer->m_3d[v + 0].position = {pos.x + vx1 + vx2, pos.y + vy1 + vy2, pos.z};
                    TempVertexBuffer->m_3d[v + 0].color = {(uint8_t)brightness, (uint8_t)brightness, (uint8_t)brightness, (uint8_t)alpha};
                    TempVertexBuffer->m_3d[v + 1].position = {pos.x + vx1 - vx2, pos.y + vy1 - vy2, pos.z};
                    TempVertexBuffer->m_3d[v + 1].color = {(uint8_t)brightness, (uint8_t)brightness, (uint8_t)brightness, (uint8_t)alpha};
                    TempVertexBuffer->m_3d[v + 2].position = {pos.x - vx1 + vx2, pos.y - vy1 + vy2, pos.z};
                    TempVertexBuffer->m_3d[v + 2].color = {(uint8_t)brightness, (uint8_t)brightness, (uint8_t)brightness, (uint8_t)alpha};
                    TempVertexBuffer->m_3d[v + 3].position = {pos.x - vx1 - vx2, pos.y - vy1 - vy2, pos.z};
                    TempVertexBuffer->m_3d[v + 3].color = {(uint8_t)brightness, (uint8_t)brightness, (uint8_t)brightness, (uint8_t)alpha};
                    
                    TempVertexBuffer->m_3d[v + 0].texCoords.u = 0.0f;
                    TempVertexBuffer->m_3d[v + 0].texCoords.v = 0.0f;
                    TempVertexBuffer->m_3d[v + 1].texCoords.u = 1.0f;
                    TempVertexBuffer->m_3d[v + 1].texCoords.v = 0.0f;
                    TempVertexBuffer->m_3d[v + 2].texCoords.u = 0.0f;
                    TempVertexBuffer->m_3d[v + 2].texCoords.v = 1.0f;
                    TempVertexBuffer->m_3d[v + 3].texCoords.u = 1.0f;
                    TempVertexBuffer->m_3d[v + 3].texCoords.v = 1.0f;

                    int ind = *TempBufferIndicesStored;
                    TempBufferRenderIndexList[ind + 0] = RubbishIndexList[0] + v;
                    TempBufferRenderIndexList[ind + 1] = RubbishIndexList[1] + v;
                    TempBufferRenderIndexList[ind + 2] = RubbishIndexList[2] + v;
                    TempBufferRenderIndexList[ind + 3] = RubbishIndexList[3] + v;
                    TempBufferRenderIndexList[ind + 4] = RubbishIndexList[4] + v;
                    TempBufferRenderIndexList[ind + 5] = RubbishIndexList[5] + v;

                    (*TempBufferVerticesStored) += 4;
                    (*TempBufferIndicesStored) += 6;
                }
            }
        
            if (*TempBufferIndicesStored != 0)
            {
                if (RwIm3DTransform(TempVertexBuffer->m_3d, *TempBufferVerticesStored, NULL, rwIM3D_VERTEXUV))
                {
                    RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, TempBufferRenderIndexList, *TempBufferIndicesStored);
                    RwIm3DEnd();
                }
            }
        }

        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)false);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)true);
        RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)false);
    }
    static void Update()
    {
        if(m_bVisible) m_nVisibility = fmin(m_nVisibility + 5, 255);
        else m_nVisibility = fmax(m_nVisibility - 5, 0);

        bool foundGround = false;
        COneSheet* sheet = StartEmptyList.m_next;
        CVector& camPos = TheCamera->GetPosition();
        if (sheet != &EndEmptyList)
        {
            float spawnDist, spawnAngle;
            spawnDist = (rand() & 0xFF) / 256.0f + RUBBISH_MAX_DIST;
            uint8_t r = rand();
            if (r & 1) spawnAngle = (rand() & 0xFF) / 256.0f * 6.28f;
            else spawnAngle = (r - 128) / 160.0f + TheCamera->m_fOrientation;

            sheet->m_basePos.x = camPos.x + spawnDist * sinf(spawnAngle);
            sheet->m_basePos.y = camPos.y + spawnDist * cosf(spawnAngle);
            sheet->m_basePos.z = FindGroundZFor3DCoord(sheet->m_basePos.x, sheet->m_basePos.y, camPos.z, &foundGround, NULL) + RUBBISH_HEIGHT_OFFSET;
        
            if (foundGround)
            {
                float level = 0.0f;
                if(GetWaterLevelNoWaves(sheet->m_basePos.x, sheet->m_basePos.y, sheet->m_basePos.z, &level, NULL, NULL) == 0)
                {
                    sheet->m_angle = (rand() & 0xFF) / 256.0f * 6.28f;
                    sheet->m_state = 1;
                    sheet->m_scale = RandomFloat(0.4f, 1.0f);
                    if (FindAttributesForCoors(sheet->m_basePos) & CAM_NO_RAIN) sheet->m_isVisible = false;
                    else sheet->m_isVisible = true;

                    sheet->RemoveFromList();
                    sheet->AddToList(&StartStaticsList);
                }
            }
        }

        bool hit = false;
        sheet = StartMoversList.m_next;
        while (sheet != &EndMoversList)
        {
            uint32_t currentTime = *m_snTimeInMilliseconds - sheet->m_moveStart;
            if (currentTime < sheet->m_moveDuration)
            {
                int32_t step = 16 * currentTime / sheet->m_moveDuration;
                int32_t stepTime = sheet->m_moveDuration / 16;
                float s = (float)(currentTime - stepTime * step) / stepTime;
                float t = (float)currentTime / sheet->m_moveDuration;
                float fxy = aAnimations[sheet->m_animationType][step] * (1.0f - s) + aAnimations[sheet->m_animationType][step + 1] * s;
                float fz = aAnimations[sheet->m_animationType][step + 17] * (1.0f - s) + aAnimations[sheet->m_animationType][step + 1 + 17] * s;
                sheet->m_animatedPos.x = sheet->m_basePos.x + fxy * sheet->m_xDist;
                sheet->m_animatedPos.y = sheet->m_basePos.y + fxy * sheet->m_yDist;
                sheet->m_animatedPos.z = (1.0f - t) * sheet->m_basePos.z + t * sheet->m_targetZ + fz * sheet->m_animHeight;
                sheet->m_angle += *ms_fTimeStep * 0.04f;
                if (sheet->m_angle > 6.28f) sheet->m_angle -= 6.28f;
                sheet = sheet->m_next;
            }
            else
            {
                sheet->m_basePos.x += sheet->m_xDist;
                sheet->m_basePos.y += sheet->m_yDist;
                sheet->m_basePos.z = sheet->m_targetZ;
                sheet->m_state = 1;
                sheet->m_isVisible = sheet->m_targetIsVisible;

                COneSheet* next = sheet->m_next;
                sheet->RemoveFromList();
                sheet->AddToList(&StartStaticsList);
                sheet = next;
            }
        }

        int32_t freq = 0;
        if (*Wind < 0.1f) freq = 31;
        else if (*Wind < 0.4f) freq = 7;
        else if (*Wind < 0.7f) freq = 1;
        else freq = 0;

        if ((*m_FrameCounter & freq) == 0)
        {
            int32_t i = rand() % NUM_RUBBISH_SHEETS;
            if (aSheets[i].m_state == 1)
            {
                aSheets[i].m_moveStart = *m_snTimeInMilliseconds;
                aSheets[i].m_moveDuration = *Wind * 1500.0f + 1000.0f;
                aSheets[i].m_animHeight = 0.2f;
                aSheets[i].m_xDist = 3.0f * *Wind;
                aSheets[i].m_yDist = 3.0f * *Wind;

                float tx = aSheets[i].m_basePos.x + aSheets[i].m_xDist;
                float ty = aSheets[i].m_basePos.y + aSheets[i].m_yDist;
                float tz = aSheets[i].m_basePos.z + 3.0f;

                aSheets[i].m_targetZ = FindGroundZFor3DCoord(tx, ty, tz, &foundGround, NULL) + RUBBISH_HEIGHT_OFFSET;
                if (FindAttributesForCoors(CVector(tx, ty, aSheets[i].m_targetZ)) & CAM_NO_RAIN) aSheets[i].m_targetIsVisible = false;
                else aSheets[i].m_targetIsVisible = true;

                if (foundGround)
                {
                    float level = 0.0f;
                    if(GetWaterLevelNoWaves(tx, ty, tz, &level, NULL, NULL) == 0)
                    {
                        aSheets[i].m_state = 2;
                        aSheets[i].m_animationType = 1;
                        aSheets[i].RemoveFromList();
                        aSheets[i].AddToList(&StartMoversList);
                    }
                }
            }
        }

        // gennariarmando's idea
        // but modified so we can "destroy" ONLY small leaves
        // NOT newspapers and NOT palm leaves (they are big!)
        for (int32_t i = 2 * NUM_RUBBISH_SHEETS_PART; i < 4 * NUM_RUBBISH_SHEETS_PART; ++i)
        {
            COneSheet* sheet = &aSheets[i];
            if(sheet->m_state != 1) continue;
            for (int32_t j = 0; j < 16; ++j) // MAX_NUM_BULLETTRACES
            {
                CBulletTrace& b = aTraces[j];
                if (b.bIsUsed && (b.End - sheet->m_basePos).Magnitude() < 0.8f)
                {
                    sheet->m_state = 0;
                    sheet->RemoveFromList();
                    sheet->AddToList(&StartEmptyList);
                    break;
                }
            }
        }

        for (int32_t i = (*m_FrameCounter % NUM_RUBBISH_SHEETS_PART) * 6; i < ((*m_FrameCounter % NUM_RUBBISH_SHEETS_PART) + 1) * 6; ++i)
        {
            if(aSheets[i].m_state == 1 && (aSheets[i].m_basePos - camPos).MagnitudeSqr2D() > sqr(RUBBISH_MAX_DIST + 1.0f))
            {
                aSheets[i].m_state = 0;
                aSheets[i].RemoveFromList();
                aSheets[i].AddToList(&StartEmptyList);
            }
        }
    }
    static void StirUp(CVehicle* veh)
    {
        if ((*m_FrameCounter ^ (veh->m_nRandomSeed & 3)) == 0) return;

        CVector& camPos = TheCamera->GetPosition();
        CVector& vehPos = veh->GetPosition();
        if (fabsf(vehPos.x - camPos.x) < RUBBISH_MAX_DIST && fabsf(vehPos.y - camPos.y) < RUBBISH_MAX_DIST)
        {
            if (fabsf(veh->m_vecMoveSpeed.x) > 0.05f || fabsf(veh->m_vecMoveSpeed.y) > 0.05f)
            {
                float speed = veh->m_vecMoveSpeed.Magnitude2D();
                if (speed > 0.05f)
                {
                    CMatrix* vehMat = veh->GetMatrix();
                    bool movingForward = DotProduct2D(veh->m_vecMoveSpeed, vehMat->up) > 0.0f;
                    COneSheet* sheet = StartStaticsList.m_next;
                    CVector2D size = GetColModel(veh)->m_boxBound.m_vecMax.m_vec2D;

                    while (sheet != &EndStaticsList)
                    {
                        COneSheet* next = sheet->m_next;
                        CVector2D carToSheet = sheet->m_basePos.m_vec2D - vehPos.m_vec2D;

                        float distFwd = DotProduct2D(carToSheet, vehMat->up);
                        if (movingForward && distFwd < -0.5f * size.y && distFwd > -1.5f * size.y || !movingForward && distFwd > 0.5f * size.y && distFwd < 1.5f * size.y)
                        {
                            float distSide = fabsf(DotProduct2D(carToSheet, vehMat->right));
                            if (distSide < 1.5 * size.x)
                            {
                                float speedToCheck = distSide < size.x ? speed : speed * 0.5f;
                                if (speedToCheck > 0.05f)
                                {
                                    sheet->m_state = 2;
                                    if (speedToCheck > 0.15f) sheet->m_animationType = 2;
                                    else sheet->m_animationType = 1;

                                    sheet->m_moveDuration = 2000;
                                    sheet->m_xDist = veh->m_vecMoveSpeed.x;
                                    sheet->m_yDist = veh->m_vecMoveSpeed.y;
                                    float dist = sqrtf(sqr(sheet->m_xDist) + sqr(sheet->m_yDist));
                                    sheet->m_xDist *= 25.0f * speed / dist;
                                    sheet->m_yDist *= 25.0f * speed / dist;
                                    sheet->m_animHeight = 3.0f * speed;
                                    sheet->m_moveStart = *m_snTimeInMilliseconds;
                                    float tx = sheet->m_basePos.x + sheet->m_xDist;
                                    float ty = sheet->m_basePos.y + sheet->m_yDist;
                                    float tz = sheet->m_basePos.z + 3.0f;
                                    sheet->m_targetZ = FindGroundZFor3DCoord(tx, ty, tz, NULL, NULL) + RUBBISH_HEIGHT_OFFSET;
                                    sheet->RemoveFromList();
                                    sheet->AddToList(&StartMoversList);
                                }
                            }
                        }
                        sheet = next;
                    }
                }
            }
        }
    }

public:
    static const uint8_t DEFAULT_RUBBISH_ALPHA = 160;
    static const int NUM_RUBBISH_SHEETS = 42;
    static const int NUM_RUBBISH_SHEETS_PART = NUM_RUBBISH_SHEETS / 6;
    static float RUBBISH_HEIGHT_OFFSET;
    static float RUBBISH_DIST_MULTIPLIER;
    static float RUBBISH_MAX_DIST;
    static float RUBBISH_FADE_DIST;
    static bool m_bVisible;
    static int m_nVisibility;
    static COneSheet aSheets[NUM_RUBBISH_SHEETS];
    static COneSheet StartEmptyList;
    static COneSheet EndEmptyList;
    static COneSheet StartStaticsList;
    static COneSheet EndStaticsList;
    static COneSheet StartMoversList;
    static COneSheet EndMoversList;
};
bool CRubbish::m_bVisible;
int CRubbish::m_nVisibility;
COneSheet CRubbish::aSheets[CRubbish::NUM_RUBBISH_SHEETS];
COneSheet CRubbish::StartEmptyList;
COneSheet CRubbish::EndEmptyList;
COneSheet CRubbish::StartStaticsList;
COneSheet CRubbish::EndStaticsList;
COneSheet CRubbish::StartMoversList;
COneSheet CRubbish::EndMoversList;
float CRubbish::RUBBISH_MAX_DIST;
float CRubbish::RUBBISH_FADE_DIST;
float CRubbish::RUBBISH_HEIGHT_OFFSET = 0.05f;
float CRubbish::RUBBISH_DIST_MULTIPLIER = 2.4f;

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Hooks
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
DECL_HOOKv(Rubbish_SetVisibility, bool bRubbishVisible)
{
    CRubbish::m_bVisible = bRubbishVisible;
}
DECL_HOOKv(InitGame)
{
    InitGame();
    CRubbish::Init();
}
DECL_HOOKv(ShutdownEngine)
{
    ShutdownEngine();
    CRubbish::Shutdown();
}
DECL_HOOKv(UpdateGame)
{
    UpdateGame();
    CRubbish::Update();
}
DECL_HOOKv(RenderEffects)
{
    RenderEffects();
    CRubbish::Render();
}
DECL_HOOKv(ProcessCarControl, CAutomobile* self)
{
    ProcessCarControl(self);
    CRubbish::StirUp(self);
}
DECL_HOOKv(ProcessCarControlVT, CAutomobile* self)
{
    ProcessCarControlVT(self);
    CRubbish::StirUp(self);
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Main
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
extern "C" void OnAllModsLoaded()
{
    logger->SetTag("RubbishPort");

    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");

    HOOKPLT(Rubbish_SetVisibility, pGTASA + BYVER(0x6758C8, 0x849418));
    HOOKPLT(InitGame, pGTASA + BYVER(0x6740A4, 0x846D20));
    HOOKPLT(ShutdownEngine, pGTASA + BYVER(0x66FF4C, 0x8402F8));
    HOOKPLT(UpdateGame, pGTASA + BYVER(0x66FE58, 0x840198));
    HOOKPLT(RenderEffects, pGTASA + BYVER(0x671980, 0x842CF0));

    HOOKPLT(ProcessCarControl, pGTASA + BYVER(0x6707E0, 0x841040));
    HOOKPLT(ProcessCarControlVT, pGTASA + BYVER(0x66D6B4, 0x83BBA8));

    aml->PlaceB(pGTASA + BYVER(0x30572A + 0x1, 0x3CABD8), pGTASA + BYVER(0x30573C + 0x1, 0x3CABE8));
    
    SET_TO(currArea, aml->GetSym(hGTASA, "_ZN5CGame8currAreaE"));
    SET_TO(m_snTimeInMilliseconds, aml->GetSym(hGTASA, "_ZN6CTimer22m_snTimeInMillisecondsE"));
    SET_TO(m_FrameCounter, aml->GetSym(hGTASA, "_ZN6CTimer14m_FrameCounterE"));
    SET_TO(TempBufferVerticesStored, aml->GetSym(hGTASA, "TempBufferVerticesStored"));
    SET_TO(TempBufferIndicesStored, aml->GetSym(hGTASA, "TempBufferIndicesStored"));
    SET_TO(TempVertexBuffer, aml->GetSym(hGTASA, "TempVertexBuffer"));
    SET_TO(TempBufferRenderIndexList, aml->GetSym(hGTASA, "TempBufferRenderIndexList"));
    SET_TO(TheCamera, aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(ms_fTimeStep, aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(Wind, aml->GetSym(hGTASA, "_ZN8CWeather4WindE"));
    SET_TO(m_fDNBalanceParam, aml->GetSym(hGTASA, "_ZN25CCustomBuildingDNPipeline17m_fDNBalanceParamE"));
    SET_TO(aTraces, aml->GetSym(hGTASA, "_ZN13CBulletTraces7aTracesE"));
    
    SET_TO(RwRenderStateSet, aml->GetSym(hGTASA, "_Z16RwRenderStateSet13RwRenderStatePv"));
    SET_TO(RwIm3DTransform, aml->GetSym(hGTASA, "_Z15RwIm3DTransformP18RxObjSpace3DVertexjP11RwMatrixTagj"));
    SET_TO(RwIm3DRenderIndexedPrimitive, aml->GetSym(hGTASA, "_Z28RwIm3DRenderIndexedPrimitive15RwPrimitiveTypePti"));
    SET_TO(RwIm3DEnd, aml->GetSym(hGTASA, "_Z9RwIm3DEndv"));
    SET_TO(FindGroundZFor3DCoord, aml->GetSym(hGTASA, "_ZN6CWorld21FindGroundZFor3DCoordEfffPbPP7CEntity"));
    SET_TO(FindAttributesForCoors, aml->GetSym(hGTASA, "_ZN10CCullZones22FindAttributesForCoorsE7CVector"));
    SET_TO(LoadTextureDatabase, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime4LoadEPKcb21TextureDatabaseFormat"));
    SET_TO(GetEntry, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime8GetEntryEPKcRb"));
    SET_TO(GetRWTexture, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime12GetRWTextureEi"));
    SET_TO(ProcessVerticalLine, aml->GetSym(hGTASA, "_ZN6CWorld19ProcessVerticalLineERK7CVectorfR9CColPointRP7CEntitybbbbbbP15CStoredCollPoly"));
    SET_TO(GetColModel, aml->GetSym(hGTASA, "_ZN7CEntity11GetColModelEv"));
    SET_TO(RwTextureDestroy, aml->GetSym(hGTASA, "_Z16RwTextureDestroyP9RwTexture"));
    SET_TO(GetWaterLevelNoWaves, aml->GetSym(hGTASA, "_ZN11CWaterLevel20GetWaterLevelNoWavesEfffPfS0_S0_"));
}