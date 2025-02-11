﻿//---------------------------------------------------------------------------
#include "stdafx.h"
#pragma hdrstop
#include "..\..\XrRender\Private\KinematicAnimatedDefs.h"
#include "SkeletonAnimated.h"
#include "UI_ActorMain.h"
//------------------------------------------------------------------------------

void CActorTools::OnObjectItemsFocused(xr_vector<ListItem*>& items)
{
    PropItemVec props;
    m_EditMode = emObject;

    // unselect
    if (m_pEditObject)
    {
        m_pEditObject->ResetSAnimation(false);
        // .StopMotion();   // убрал из-за того что не миксятся анимации в режиме енжине
        m_pEditObject->SelectBones(false);
    }
    for (ListItem* prop : items)
    {
        if (prop)
        {
            m_EditMode = EEditMode(prop->Type());
            switch (m_EditMode)
            {
                case emObject:
                {
                    m_Props->ClearProperties();
                    FillObjectProperties(props, OBJECT_PREFIX, prop);
                }
                    break;
                case emMotion:
                {
                    m_Props->ClearProperties();
                    LPCSTR m_name = ExtractMotionName(prop->Key());
                    u16    slot   = ExtractMotionSlot(prop->Key());
                    FillMotionProperties(props, MOTIONS_PREFIX, prop);
                    SetCurrentMotion(m_name, slot);
                }
                break;
                case emBone:
                {
                    m_Props->ClearProperties();
                    FillBoneProperties(props, BONES_PREFIX, prop);
                    CBone* BONE = (CBone*)prop->m_Object;
                    if (BONE)
                        BONE->Select(TRUE);
                }
                break;
                case emSurface:
                {
                    m_Props->ClearProperties();
                    FillSurfaceProperties(props, SURFACES_PREFIX, prop);
                }
                    break;
                case emMesh:
                    break;
            }
        }
    }

    m_Props->AssignItems(props);
    UI->RedrawScene();
}
//------------------------------------------------------------------------------

void CActorTools::OnChangeTransform(PropValue* sender)
{
    OnMotionKeysModified();
    UI->RedrawScene();
}
//------------------------------------------------------------------------------
void CActorTools::OnExportImportRefsClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    switch (V->btn_num)
    {
        case 0: {   // export
            xr_string fname;
            if (EFS.GetSaveName(_import_, fname))
            {
                CInifile                        ini(fname.c_str(), FALSE, FALSE, FALSE);
                xr_vector<shared_str>::iterator it   = m_pEditObject->m_SMotionRefs.begin();
                xr_vector<shared_str>::iterator it_e = m_pEditObject->m_SMotionRefs.end();
                string64                        buff;
                for (u32 i = 0; it != it_e; ++it, ++i)
                {
                    sprintf(buff, "%06d", i);
                    ini.w_string("refs", buff, (*it).c_str());
                }
                ini.save_as(fname.c_str());
                bModif = false;
            }
        }
        break;
        case 1: {   // import
            xr_string fname;
            if (EFS.GetOpenName(EDevice->m_hWnd, _import_, fname, false))
            {
                CInifile ini(fname.c_str(), TRUE, TRUE, FALSE);
                m_pEditObject->m_SMotionRefs.clear();
                CInifile::Sect&   S    = ini.r_section("refs");
                CInifile::SectCIt it   = S.Data.begin();
                CInifile::SectCIt it_e = S.Data.end();
                for (; it != it_e; ++it)
                {
                    m_pEditObject->m_SMotionRefs.push_back(it->second);
                }

                OnMotionKeysModified();
                ExecCommand(COMMAND_UPDATE_PROPERTIES);
                bModif = true;
            }
            else
                bModif = false;
        }
        break;
    };
}

void CActorTools::OnMotionEditClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    xr_string fn;
    switch (V->btn_num)
    {
        case 0:
        {   // append
            xr_string folder, nm, full_name;
            xr_string fnames;
            if (EFS.GetOpenName(EDevice->m_hWnd, _smotion_, fnames, true))
            {
                AStringVec lst;
                _SequenceToList(lst, fnames.c_str());
                bool bRes = false;
                for (AStringIt it = lst.begin(); it != lst.end(); it++)
                {
                    if (AppendMotion(it->c_str()))
                        bRes = true;
                }
                ExecCommand(COMMAND_UPDATE_PROPERTIES);
                if (bRes)
                    OnMotionKeysModified();
                else
                    ELog.DlgMsg(mtError, "! Append not completed.");
                bModif = false;
            }
            else
                bModif = false;
        }
        break;
        case 1:
        {   // delete
            ListItemsVec items;
            if (m_ObjectItems->GetSelected(MOTIONS_PREFIX, items, true))
            {
                if (ELog.DlgMsg(mtConfirmation, mbYes | mbNo, "& Delete selected %d item(s)?", items.size()) == mrYes)
                {
                    for (ListItemsIt it = items.begin(); it != items.end(); it++)
                    {
                        VERIFY((*it)->m_Object);
                        RemoveMotion(((CSMotion*)(*it)->m_Object)->Name());
                    }
                    SelectListItem(MOTIONS_PREFIX, 0, true, false, false);
                    ExecCommand(COMMAND_UPDATE_PROPERTIES);
                    OnMotionKeysModified();
                    bModif = false;
                }
                else
                {
                    bModif = false;
                }
            }
            else
                ELog.DlgMsg(mtInformation, "# Select at least one motion.");
        }
        break;
        case 2:
        {   // save
            int mr = ELog.DlgMsg(mtConfirmation, "- Save selected motions only?");
            if (mr != mrCancel)
            {
                if (EFS.GetSaveName(_smotion_, fn, 0, 1))
                {
                    switch (mr)
                    {
                        case mrYes:
                            SaveMotions(fn.c_str(), true);
                            break;
                        case mrNo:
                            SaveMotions(fn.c_str(), false);
                            break;
                    }
                }
            }
            bModif = false;
        }
        break;
    }
}

void CActorTools::RealUpdateProperties()
{
    m_Flags.set(flRefreshProps, FALSE);
    ListItemsVec items;
    if (m_pEditObject)
    {
        LHelper().CreateItem(items, OBJECT_PREFIX, 0, emObject);
        m_pEditObject->FillSurfaceList(SURFACES_PREFIX, items, emSurface);
        // skin
        if (m_pEditObject->IsSkeleton())
        {
            if (m_pEditObject->m_SMotionRefs.size())
            {
                m_RenderObject.FillMotionList(MOTIONS_PREFIX, items, emMotion);
            }
            else
            {
                m_pEditObject->FillMotionList(MOTIONS_PREFIX, items, emMotion);
            }
            m_pEditObject->FillBoneList(BONES_PREFIX, items, emBone);
        }
    }

    m_ObjectItems->AssignItems(items, nullptr, true, true);   //,"",true);
    // if appended motions exist - select it
    if (!appended_motions.empty())
    {
        SelectListItem(MOTIONS_PREFIX, 0, true, false, true);
        for (SMotionIt m_it = appended_motions.begin(); m_it != appended_motions.end(); m_it++)
            SelectListItem(MOTIONS_PREFIX, (*m_it)->Name(), true, true, true);
        appended_motions.clear();
    }
}
//------------------------------------------------------------------------------

void CActorTools::OnMotionTypeChange(PropValue* sender)
{
    RefreshSubProperties();
}
//------------------------------------------------------------------------------

void CActorTools::OnMotionNameChange(PropValue* V)
{
    OnMotionKeysModified();
    UpdateProperties();
}
//------------------------------------------------------------------------------

void CActorTools::OnMotionControlClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    switch (V->btn_num)
    {
        case 0:
            PlayMotion();
            break;
        case 1:
            StopMotion();
            break;
        case 2:
            PauseMotion();
            break;
    }
    bModif = false;
}
//------------------------------------------------------------------------------
void CActorTools::OnMarksControlClick12(ButtonValue* V, bool& bModif, bool& bSafe)
{
    switch (V->btn_num)
    {
        case 0:
            AddMarksChannel(true);
            break;
        case 1:
            RemoveMarksChannel(true);
            break;
    }
    bModif = true;
}
//------------------------------------------------------------------------------

void CActorTools::OnMarksControlClick34(ButtonValue* V, bool& bModif, bool& bSafe)
{
    switch (V->btn_num)
    {
        case 0:
            AddMarksChannel(false);
            break;
        case 1:
            RemoveMarksChannel(false);
            break;
    }
    bModif = true;
}
//------------------------------------------------------------------------------

void CActorTools::OnMotionRefsChange(PropValue* sender)
{
    u32 set_cnt = _GetItemCount(m_tmp_mot_refs.c_str());

    m_pEditObject->m_SMotionRefs.clear();
    m_pEditObject->m_SMotionRefs.reserve(set_cnt);

    string_path nm;
    for (u32 k = 0; k < set_cnt; ++k)
    {
        _GetItem(m_tmp_mot_refs.c_str(), k, nm);
        m_pEditObject->m_SMotionRefs.push_back(nm);
    }

    OnMotionKeysModified();
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}

void CActorTools::OnBoxAxisClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    CBone* BONE = (CBone*)V->tag;
    switch (V->btn_num)
    {
        case 0:
            BONE->shape.box.m_rotate.k.set(1, 0, 0);
            break;
        case 1:
            BONE->shape.box.m_rotate.k.set(0, 1, 0);
            break;
        case 2:
            BONE->shape.box.m_rotate.k.set(0, 0, 1);
            break;
    }
    Fvector::generate_orthonormal_basis_normalized(
        BONE->shape.box.m_rotate.k, BONE->shape.box.m_rotate.j, BONE->shape.box.m_rotate.i);
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}

void CActorTools::OnCylinderAxisClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    CBone* BONE = (CBone*)V->tag;
    switch (V->btn_num)
    {
        case 0:
            BONE->shape.cylinder.m_direction.set(1, 0, 0);
            break;
        case 1:
            BONE->shape.cylinder.m_direction.set(0, 1, 0);
            break;
        case 2:
            BONE->shape.cylinder.m_direction.set(0, 0, 1);
            break;
    }
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}

extern ECORE_API BOOL g_force16BitTransformQuant;
extern ECORE_API BOOL g_forceFloatTransformQuant;
#include "envelope.h"
Fvector StartMotionPoint, EndMotionPoint;
void    CActorTools::FillMotionProperties(PropItemVec& items, LPCSTR pref, ListItem* sender)
{
    R_ASSERT(m_pEditObject);
    CSMotion*  SM = m_pEditObject->m_SMotionRefs.size() ? 0 : (CSMotion*)sender->m_Object;
    PropValue* V;

    xr_string m_cnt;
    if (m_pEditObject->m_SMotionRefs.size())
    {
        if (MainForm->GetLeftBarForm()->GetRenderMode() == UILeftBarForm::Render_Engine)
        {
            CKinematicsAnimated* V = PKinematicsAnimated(m_RenderObject.m_pVisual);
            if (V)
                m_cnt = V->LL_CycleCount() + V->LL_FXCount();
        }
        else
        {
            m_cnt = xr_string(m_pEditObject->SMotionCount()) + " (Inaccessible)";
        }
    }
    else
    {
        m_cnt = m_pEditObject->SMotionCount();
    }

    PHelper().CreateCaption(items, PrepareKey(pref, "Global\\Motion count"), m_cnt.c_str());

    xr_string tmp;
    for (u32 i = 0; i < m_pEditObject->m_SMotionRefs.size(); ++i)
    {
        if (i != 0)
            tmp += ',';
        tmp += m_pEditObject->m_SMotionRefs[i].c_str();
    }
    m_tmp_mot_refs = tmp.c_str();
    V = PHelper().CreateChoose(items, PrepareKey(pref, "Global\\Motion reference"), &m_tmp_mot_refs, smGameSMotions, 0, 0, MAX_ANIM_SLOT);
    // m_pEditObject->m_SMotionRefs
    V->OnChangeEvent.bind(this, &CActorTools::OnMotionRefsChange);
    ButtonValue* B;

    B = PHelper().CreateButton(items, PrepareKey(pref, "Export Import"), "ExportRefs,ImportRefs", ButtonValue::flFirstOnly);
    B->OnBtnClickEvent.bind(this, &CActorTools::OnExportImportRefsClick);

    if (m_pEditObject->m_SMotionRefs.size() == 0)
    {
        B = PHelper().CreateButton(items, PrepareKey(pref, "Global\\Edit"), "Append,Delete,Save", ButtonValue::flFirstOnly);
        B->OnBtnClickEvent.bind   (this, &CActorTools::OnMotionEditClick);

        V = PHelper().CreateBOOL(items, PrepareKey(pref, "MotionExport\\Force 16bit Motion"), &g_force16BitTransformQuant);
        V->Owner()->hint_text = 
            "Export animations 16 bit - CoP Format, good quality.\n If nothing is selected, animations will be exported\n 8 bit - SoC Format, poor quality."_RU >
            u8"Экспорт анимаций 16 bit - CoP Формат, хорошее качество.\n Если ничего не выбрано - анимации будут экспортированы\n 8 bit - SoC Формат, плохое качество.";
        V->OnChangeEvent.bind(this, &CActorTools::OnMotionCompressionChanged);

        V = PHelper().CreateBOOL(items, PrepareKey(pref, "MotionExport\\No Compress Motion"), &g_forceFloatTransformQuant);
        V->Owner()->hint_text = 
            "No compress - New uncompressed format, better quality.\n To support such animations, engine edits are required.\n If nothing is selected, animations will be exported\n 8 bit - SoC Format, poor quality."_RU >
            u8"No compress - Новый формат без сжатия, лучшее качество.\n Для поддержки таких анимаций - требуются движковые правки.\n Если ничего не выбрано - анимации будут экспортированы\n 8 bit - SoC Формат, плохое качество.";
        V->OnChangeEvent.bind(this, &CActorTools::OnMotionCompressionChanged);
    }
    if (SM)
    {
        B = PHelper().CreateButton(items, PrepareKey(pref, "Motion\\Control"), "Play,Stop,Pause", ButtonValue::flFirstOnly);
        B->OnBtnClickEvent.bind   (this, &CActorTools::OnMotionControlClick);
        PHelper().CreateCaption   (items, PrepareKey(pref, "Motion\\Frame\\Start"), shared_str().printf("%d", SM->FrameStart()));
        PHelper().CreateCaption   (items, PrepareKey(pref, "Motion\\Frame\\End"), shared_str().printf("%d", SM->FrameEnd()));
        PHelper().CreateCaption   (items, PrepareKey(pref, "Motion\\Frame\\Length"), shared_str().printf("%d", SM->Length()));
        PropValue* P = 0;
        P = PHelper().CreateName  (items, PrepareKey(pref, "Motion\\Name"), &SM->name, sender);
        P->OnChangeEvent.bind     (this, &CActorTools::OnMotionNameChange);
        PHelper().CreateFloat     (items, PrepareKey(pref, "Motion\\Speed"), &SM->fSpeed, 0.f, 10.f, 0.01f, 2);
        PHelper().CreateFloat     (items, PrepareKey(pref, "Motion\\Accrue"), &SM->fAccrue, 0.f, 10.f, 0.01f, 2);
        PHelper().CreateFloat     (items, PrepareKey(pref, "Motion\\Falloff"), &SM->fFalloff, 0.f, 10.f, 0.01f, 2);

        PropValue* TV = 0;
        TV = PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Type FX"), &SM->m_Flags, esmFX);
        TV->OnChangeEvent.bind(this, &CActorTools::OnMotionTypeChange);
        m_BoneParts.clear();
        if (SM->m_Flags.is(esmFX))
        {
            for (BoneIt it = m_pEditObject->FirstBone(); it != m_pEditObject->LastBone(); it++)
                m_BoneParts.push_back(xr_rtoken((*it)->Name().c_str(), (*it)->SelfID));
            PHelper().CreateRToken16(items, PrepareKey(pref, "Motion\\FX\\Start bone"), (u16*)&SM->m_BoneOrPart, &*m_BoneParts.begin(), m_BoneParts.size());
            PHelper().CreateFloat(items, PrepareKey(pref, "Motion\\FX\\Power"), &SM->fPower, 0.f, 10.f, 0.01f, 2);
        }
        else
        {
            m_BoneParts.push_back(xr_rtoken("--all bones--", BI_NONE));
            for (BPIt it = m_pEditObject->FirstBonePart(); it != m_pEditObject->LastBonePart(); it++)
                m_BoneParts.push_back(xr_rtoken(it->alias.c_str(), it - m_pEditObject->FirstBonePart()));
            PHelper().CreateRToken16(items, PrepareKey(pref, "Motion\\Cycle\\Bone part"), &SM->m_BoneOrPart, &*m_BoneParts.begin(),  m_BoneParts.size());
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\Stop at end"), &SM->m_Flags, esmStopAtEnd);
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\No mix"), &SM->m_Flags, esmNoMix);
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\Sync part"), &SM->m_Flags, esmSyncPart);

            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\UseFootSteps"), &SM->m_Flags, esmUseFootSteps);
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\Move XForm"), &SM->m_Flags, esmRootMover);
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\Idle"), &SM->m_Flags, esmIdle);
            PHelper().CreateFlag8(items, PrepareKey(pref, "Motion\\Cycle\\UseWeaponBone"), &SM->m_Flags, esmUseWeaponBone);
        }

        B = PHelper().CreateButton(items, PrepareKey(pref, "Marks\\Control-12"), "Add,Remove", ButtonValue::flFirstOnly);
        B->OnBtnClickEvent.bind(this, &CActorTools::OnMarksControlClick12);

        B = PHelper().CreateButton(items, PrepareKey(pref, "Marks\\Control-34"), "Add,Remove", ButtonValue::flFirstOnly);
        B->OnBtnClickEvent.bind(this, &CActorTools::OnMarksControlClick34);

        for (u32 i = 0; i < SM->marks.size(); ++i)
        {
            string128 buff;
            sprintf(buff, "Marks\\%d", i);
            P = PHelper().CreateCaption(items, PrepareKey(pref, buff), SM->marks[i].name);
        }

        {
            int            bidx = 0;
            BoneMotionVec& BMV  = SM->BoneMotions();
            st_BoneMotion& BM   = BMV[bidx];
            StartMotionPoint.x  = BM.envs[ctPositionX]->keys.size() ? BM.envs[ctPositionX]->keys.front()->value : -1;
            StartMotionPoint.y  = BM.envs[ctPositionY]->keys.size() ? BM.envs[ctPositionY]->keys.front()->value : -1;
            StartMotionPoint.z  = BM.envs[ctPositionZ]->keys.size() ? BM.envs[ctPositionZ]->keys.front()->value : -1;

            PHelper().CreateVector(items, PrepareKey(pref, "Motion\\RootStartTransform"), &StartMotionPoint, -10000, 10000, 0.001, 4);

            EndMotionPoint.x = BM.envs[ctPositionX]->keys.size() ? BM.envs[ctPositionX]->keys.back()->value : -1;
            EndMotionPoint.y = BM.envs[ctPositionY]->keys.size() ? BM.envs[ctPositionY]->keys.back()->value : -1;
            EndMotionPoint.z = BM.envs[ctPositionZ]->keys.size() ? BM.envs[ctPositionZ]->keys.back()->value : -1;

            PHelper().CreateVector(items, PrepareKey(pref, "Motion\\RootEndTransform"), &EndMotionPoint, -10000, 10000, 0.001, 4);
        }
    }
}
//------------------------------------------------------------------------------

xr_token joint_types[] =
{
    {"Rigid", jtRigid},
    {"Cloth", jtCloth},
    {"Joint", jtJoint},
    {"Wheel [Steer-X/Roll-Z]", jtWheel},
    {"Slider", jtSlider},
    //	{ "Wheel [Steer-X/Roll-Z]", jtWheelXZ	},
    //	{ "Wheel [Steer-X/Roll-Y]", jtWheelXY	},
    //	{ "Wheel [Steer-Y/Roll-X]", jtWheelYX	},
    //	{ "Wheel [Steer-Y/Roll-Z]", jtWheelYZ	},
    //	{ "Wheel [Steer-Z/Roll-X]", jtWheelZX	},
    //	{ "Wheel [Steer-Z/Roll-Y]", jtWheelZY	},
    {0, 0}
};

xr_token shape_types[] =
{
    {"None", SBoneShape::stNone},
    {"Box", SBoneShape::stBox},
    {"Sphere", SBoneShape::stSphere},
    {"Cylinder", SBoneShape::stCylinder},
    {0, 0}
};

static const LPCSTR axis[3] = {"Axis X", "Axis Y", "Axis Z"};

void CActorTools::OnJointTypeChange(PropValue* V)
{
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}
void CActorTools::OnShapeTypeChange(PropValue* V)
{
    UI->RedrawScene();
    ExecCommand(COMMAND_UPDATE_PROPERTIES);
}
void CActorTools::OnBindTransformChange(PropValue* V)
{
    R_ASSERT(m_pEditObject);
    m_pEditObject->OnBindTransformChange();
    UI->RedrawScene();
}

void CActorTools::OnTypeChange(PropValue* V)
{
    u32 current_type = m_pEditObject->m_objectFlags.flags & (CEditableObject::eoDynamic | CEditableObject::eoHOM | CEditableObject::eoSoundOccluder | CEditableObject::eoMultipleUsage);
    m_pEditObject->m_objectFlags.flags = m_pEditObjectType;

    if (current_type != m_pEditObjectType)
    {
        if (m_pEditObjectType == CEditableObject::eoMultipleUsage)
        {
            m_pEditObject->m_objectFlags.flags |= CEditableObject::eoUsingLOD;
        }
    }
    RefreshSubProperties();
}

void CActorTools::OnUsingLodFlagChange(PropValue* V)
{
    RefreshSubProperties();
}

void CActorTools::OnMakeThumbnailClick(ButtonValue* sender, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    switch (sender->btn_num)
    {
        case 0:
        {
            MakeThumbnail();
        }
        break;
    }
}

void CActorTools::OnMakeLODClick(ButtonValue* sender, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    switch (sender->btn_num)
    {
        case 0:
        {
            GenerateLOD(true);
        }
        case 1:
        {
            GenerateLOD(false);
        }
        break;
    }
}

void CActorTools::OnBoneShapeClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    switch (V->btn_num)
    {
        case 0:
            m_pEditObject->GenerateBoneShape(false);
            break;
        case 1:
            m_pEditObject->GenerateBoneShape(true);
            break;
    }
}

void GetBindAbsolutePosition(CBone* B, Fmatrix& dest)
{
    if (B->Parent())
        GetBindAbsolutePosition(B->Parent(), dest);

    {
        Fmatrix M;
        M.setXYZi(B->_RestRotate());
        M.c.set(B->_RestOffset());
        M.mulA_43(dest);
        dest.set(M);
    }
}

void CActorTools::OnBoneCreateDeleteClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    BoneVec sel_bones;
    m_pEditObject->GetSelectedBones(sel_bones);
    switch (V->btn_num)
    {
        case 0:
        {   // create
            CBone* B = sel_bones.size() ? sel_bones[0] : NULL;
            m_pEditObject->AddBone(B);
            bModif = true;
        }
        break;
        case 1:
        {   // deleet
            if (sel_bones.size() != 1)
            {
                Msg("~ Select 1 bone please.");
                return;
            }
            if (ELog.DlgMsg(mtConfirmation, mbYes | mbNo, "& Delete selected bone?") == mrYes)
            {
                m_pEditObject->DeleteBone(sel_bones[0]);
                bModif = true;
            }
        }
        break;
        case 2:
        {
            if (sel_bones.size() != 1)
            {
                Msg("~ Select 1 bone please.");
                return;
            }
            LPCSTR _bone_name = 0;
            UIChooseForm::SelectItem(smSkeletonBonesInObject, 1, 0, 0, m_pEditObject);
            m_ChooseSkeletonBones = true;
        }
        break;
    }
    if (bModif)
    {
        bSafe = false;
        m_Flags.set(flRefreshProps, TRUE);
    }
}

void CActorTools::OnDrawUI()
{
    if (m_ChooseSkeletonBones)
    {
        xr_string str;
        bool      ok;
        if (UIChooseForm::GetResult(ok, str))
        {
            if (ok)
            {
                BoneVec sel_bones;
                m_pEditObject->GetSelectedBones(sel_bones);
                Msg("# selected bone %s", str.c_str());
                CBone* BSelected = m_pEditObject->FindBoneByName(str.c_str());
                R_ASSERT(BSelected);

                CBone*  BEditable = sel_bones[0];
                Fmatrix matrix1, matrix2;
                Fvector offset, rotate1, rotate2, rotate;
                float   length = 0.01f;

                m_pEditObject->GotoBindPose();

                matrix1.identity();
                matrix2.identity();
                GetBindAbsolutePosition(BEditable, matrix1);
                GetBindAbsolutePosition(BSelected, matrix2);

                Fmatrix R;
                R.mul(matrix1.invert(), matrix2).getXYZi(rotate);
                offset.set(R.c);

                BEditable->SetRestParams(length, offset, rotate);

                m_pEditObject->GotoBindPose();
            }
            m_ChooseSkeletonBones = false;
            UIChooseForm::Update();
        }
    }
}

bool CActorTools::OnBoneNameAfterEdit(PropValue* sender, shared_str& edit_val)
{
    R_ASSERT(m_pEditObject);
    BoneVec sel_bones;
    m_pEditObject->GetSelectedBones(sel_bones);
    CBone* B = sel_bones.size() ? sel_bones[0] : NULL;
    R_ASSERT(B);
    for (auto& bone : m_pEditObject->Bones())
    {
        if (bone->Name() == edit_val)
            return false;
    }
    m_pEditObject->RenameBone(B, edit_val.c_str());
    return true;
}

void CActorTools::OnBoneNameChangeEvent(PropValue* sender)
{
    UpdateProperties();
}

void CActorTools::OnBoneEditClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    switch (V->btn_num)
    {
        case 0:
            m_pEditObject->GotoBindPose();
            ExecCommand(COMMAND_UPDATE_PROPERTIES);
            bModif = false;
            break;
        case 1:
            if (ELog.DlgMsg(mtConfirmation, "# Are you sure to reset IK data?") == mrYes)
                m_pEditObject->ResetBones();
            bModif = true;
            break;
        case 2:
            m_pEditObject->ClampByLimits(false);
            ExecCommand(COMMAND_UPDATE_PROPERTIES);
            bModif = false;
            break;
    }
}

void CActorTools::OnBoneFileClick(ButtonValue* V, bool& bModif, bool& bSafe)
{
    R_ASSERT(m_pEditObject);
    switch (V->btn_num)
    {
        case 0:
        {
            xr_string fn;
            if (EFS.GetOpenName(EDevice->m_hWnd, "$sbones$", fn))
            {
                IReader* R = FS.r_open(fn.c_str());
                if (m_pEditObject->LoadBoneData(*R))
                    ELog.DlgMsg(mtInformation, "+ Bone data succesfully loaded.");
                else
                    ELog.DlgMsg(mtError, "! Failed to load bone data.");
                FS.r_close(R);
            }
            else
            {
                bModif = false;
            }
        }
        break;
        case 1:
        {
            xr_string fn;
            if (EFS.GetSaveName("$sbones$", fn))
            {
                IWriter* W = FS.w_open(fn.c_str());
                if (W)
                {
                    m_pEditObject->SaveBoneData(*W);
                    FS.w_close(W);
                }
                else
                {
                    Log("! Can't save skeleton bones:", fn.c_str());
                }
                bModif = false;
            }
        }
        break;
    }
}

void CActorTools::OnBoneLimitsChange(PropValue* sender)
{
    m_pEditObject->ClampByLimits(true);
}

void CActorTools::OnMotionCompressionChanged(PropValue* sender)
{
  BOOLValue * casted = dynamic_cast<BOOLValue*>(sender);

  if (g_force16BitTransformQuant && g_forceFloatTransformQuant)
  {
    if(casted->value == &g_force16BitTransformQuant)
      g_forceFloatTransformQuant = false;
    else
      g_force16BitTransformQuant = false;
  }
}

void CActorTools::FillBoneProperties(PropItemVec& items, LPCSTR pref, ListItem* sender)
{
    R_ASSERT(m_pEditObject);
    CBone* BONE = (CBone*)sender->m_Object;

    PHelper().CreateCaption(items, PrepareKey(pref, "Global\\Bone count"), shared_str().printf("%d", m_pEditObject->BoneCount()));
    ButtonValue* B;
    B = PHelper().CreateButton(items, PrepareKey(pref, "Global\\File"), "Load,Save", ButtonValue::flFirstOnly);
    B->OnBtnClickEvent.bind(this, &CActorTools::OnBoneFileClick);
    B = PHelper().CreateButton(items, PrepareKey(pref, "Global\\Edit"), "Bind pose,Reset IK,Clamp limits", ButtonValue::flFirstOnly);
    B->OnBtnClickEvent.bind(this, &CActorTools::OnBoneEditClick);
    B = PHelper().CreateButton(items, PrepareKey(pref, "Global\\Generate Shape"), "All, Selected", ButtonValue::flFirstOnly);
    B->OnBtnClickEvent.bind(this, &CActorTools::OnBoneShapeClick);
    //---
    B = PHelper().CreateButton(items, PrepareKey(pref, "Global\\Bone"), "Create, Delete, Orient", ButtonValue::flFirstOnly);
    B->OnBtnClickEvent.bind(this, &CActorTools::OnBoneCreateDeleteClick);
    //---

    if (BONE)
    {
        PropValue* V;
        PHelper().CreateCaption(items, PrepareKey(pref, "Bone\\Name"), BONE->Name());
        PHelper().CreateNameCB(items, PrepareKey(pref, "Bone\\NameEditable"), &BONE->NameRef(), 0, 0, RTextValue::TOnAfterEditEvent(this, &CActorTools::OnBoneNameAfterEdit))->OnChangeEvent.bind(this, &CActorTools::OnBoneNameChangeEvent);

        // PHelper().CreateCaption(items, PrepareKey(pref,"Bone\\Influence"), shared_str().sprintf("%d vertices",0));
        PHelper().CreateChoose(items, PrepareKey(pref, "Bone\\Game Material"), &BONE->game_mtl, smGameMaterial);
        PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Mass"), &BONE->mass, 0.f, 10000.f);
        PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Center Of Mass"), &BONE->center_of_mass, -10000.f, 10000.f);
        V = PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Bind Position"), &BONE->_RestOffset(), -10000.f, 10000.f);
        V->OnChangeEvent.bind(this, &CActorTools::OnBindTransformChange);
        V = PHelper().CreateAngle3(items, PrepareKey(pref, "Bone\\Bind Rotation"), &BONE->_RestRotate());
        V->OnChangeEvent.bind(this, &CActorTools::OnBindTransformChange);
        PHelper().CreateFlag16(items, PrepareKey(pref, "Bone\\Shape\\Flags\\No Pickable"), &BONE->shape.flags, SBoneShape::sfNoPickable);
        PHelper().CreateFlag16(items, PrepareKey(pref, "Bone\\Shape\\Flags\\No Physics"), &BONE->shape.flags, SBoneShape::sfNoPhysics);
        PHelper().CreateFlag16(items, PrepareKey(pref, "Bone\\Shape\\Flags\\Remove After Break"), &BONE->shape.flags, SBoneShape::sfRemoveAfterBreak);
        PHelper().CreateFlag16( items, PrepareKey(pref, "Bone\\Shape\\Flags\\No Fog Collider"), &BONE->shape.flags, SBoneShape::sfNoFogCollider);

        V = PHelper().CreateToken16(items, PrepareKey(pref, "Bone\\Shape\\Type"), &BONE->shape.type, shape_types);
        V->OnChangeEvent.bind(this, &CActorTools::OnShapeTypeChange);
        switch (BONE->shape.type)
        {
            case SBoneShape::stBox:
                PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Shape\\Box\\Center"), &BONE->shape.box.m_translate, -10000.f, 10000.f);
                B = PHelper().CreateButton(items, PrepareKey(pref, "Bone\\Shape\\Box\\Align Axis"), "X,Y,Z", 0);
                B->OnBtnClickEvent.bind(this, &CActorTools::OnBoxAxisClick);
                B->tag = (size_t)BONE;
                PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Shape\\Box\\Half Size"), &BONE->shape.box.m_halfsize, 0.f, 1000.f);
                break;
            case SBoneShape::stSphere:
                PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Shape\\Sphere\\Position"), &BONE->shape.sphere.P, -10000.f, 10000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Shape\\Sphere\\Radius"), &BONE->shape.sphere.R, 0.f, 1000.f);
                break;
            case SBoneShape::stCylinder:
                PHelper().CreateVector(items, PrepareKey(pref, "Bone\\Shape\\Cylinder\\Center"), &BONE->shape.cylinder.m_center, -10000.f, 10000.f);
                B = PHelper().CreateButton(items, PrepareKey(pref, "Bone\\Shape\\Cylinder\\Align Axis"), "X,Y,Z", 0);
                B->OnBtnClickEvent.bind(this, &CActorTools::OnCylinderAxisClick);
                B->tag = (size_t)BONE;
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Shape\\Cylinder\\Height"), &BONE->shape.cylinder.m_height, 0.f, 1000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Shape\\Cylinder\\Radius"), &BONE->shape.cylinder.m_radius, 0.f, 1000.f);
                break;
        }

        Fvector lim_rot;
        Fmatrix mLocal, mBind, mBindI;
        mBind.setXYZi(BONE->_RestRotate().x, BONE->_RestRotate().y, BONE->_RestRotate().z);
        mBindI.invert(mBind);
        mLocal.setXYZi(BONE->_Rotate().x, BONE->_Rotate().y, BONE->_Rotate().z);
        mLocal.mulA_43(mBindI);
        mLocal.getXYZi(lim_rot);
        lim_rot.x = rad2deg(lim_rot.x);
        lim_rot.y = rad2deg(lim_rot.y);
        lim_rot.z = rad2deg(lim_rot.z);

        PHelper().CreateCaption(items, PrepareKey(pref, "Bone\\Joint\\Current Rotation"), shared_str().printf("{%3.2f, %3.2f, %3.2f}", VPUSH(lim_rot)));
        SJointIKData& data = BONE->IK_data;
        V = PHelper().CreateFlag32(items, PrepareKey(pref, "Bone\\Joint\\Breakable"), &data.ik_flags, SJointIKData::flBreakable);
        V->OnChangeEvent.bind(this, &CActorTools::OnJointTypeChange);
        if (data.ik_flags.is(SJointIKData::flBreakable))
        {
            PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Break Force"), &data.break_force, 0.f, 1000000000.f);
            PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Break Torque"), &data.break_torque, 0.f, 1000000000.f);
        }
        V = PHelper().CreateToken32(items, PrepareKey(pref, "Bone\\Joint\\Type"), (u32*)&data.type, joint_types);
        V->OnChangeEvent.bind(this, &CActorTools::OnJointTypeChange);
        switch (data.type)
        {
            case jtRigid:
                break;
            case jtCloth:
            {
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Friction"), &data.friction, 0.f, 1000000000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Spring Factor"), &data.spring_factor, 0.f, 1000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Damping Factor"), &data.damping_factor, 0.f, 1000.f);
            }
            break;
            case jtJoint:
            {
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Friction"), &data.friction, 0.f, 1000000000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Spring Factor"), &data.spring_factor, 0.f, 1000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Damping Factor"), &data.damping_factor, 0.f, 1000.f);
                for (int k = 0; k < 3; k++)
                {
                    V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Limits", axis[k], "Min"), &data.limits[k].limit.x, -M_PI, 0.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Limits", axis[k], "Max"), &data.limits[k].limit.y, 0.f, M_PI);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Limits", axis[k], "Spring Factor"), &data.limits[k].spring_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Limits", axis[k], "Damping Factor"), &data.limits[k].damping_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                }
            }
            break;
            case jtWheel:
            {
                //	        int idx = (data.type-jtWheelXZ)/2;
                int idx = (data.type - jtWheel) / 2;
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Friction"), &data.friction, 0.f, 1000000000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Spring Factor"), &data.spring_factor, 0.f, 1000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Damping Factor"), &data.damping_factor, 0.f, 1000.f);
                V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Steer\\Limits Min"), &data.limits[idx].limit.x, -PI_DIV_2, 0.f);
                V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Steer\\Limits Max"), &data.limits[idx].limit.y, 0, PI_DIV_2);
                V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
            }
            break;
            case jtSlider:
            {
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Friction"), &data.friction, 0.f, 1000000000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Spring Factor"), &data.spring_factor, 0.f, 1000.f);
                PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Damping Factor"), &data.damping_factor, 0.f, 1000.f);
                {   // slider
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Slide (Axis Z)\\Limits Min"), &data.limits[0].limit[0], -100.f, 0.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Slide (Axis Z)\\Limits Max"), &data.limits[0].limit[1], 0.f, 100.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Slide (Axis Z)\\Spring Factor"), &data.limits[0].spring_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Slide (Axis Z)\\Damping Factor"), &data.limits[0].damping_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                }
                {   // rotate
                    V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Rotate (Axis Z)\\Limits Min"), &data.limits[1].limit[0], -M_PI, 0.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateAngle(items, PrepareKey(pref, "Bone\\Joint\\Rotate (Axis Z)\\Limits Max"), &data.limits[1].limit[1], 0.f, M_PI);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Rotate (Axis Z)\\Spring Factor"), &data.limits[1].spring_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                    V = PHelper().CreateFloat(items, PrepareKey(pref, "Bone\\Joint\\Rotate (Axis Z)\\Damping Factor"), &data.limits[1].damping_factor, 0.f, 1000.f);
                    V->OnChangeEvent.bind(this, &CActorTools::OnBoneLimitsChange);
                }
            }
            break;
        }
    }
}
//------------------------------------------------------------------------------

void CActorTools::FillSurfaceProperties(PropItemVec& items, LPCSTR pref, ListItem* sender)
{
    R_ASSERT(m_pEditObject);
    CSurface* SURF = (CSurface*)sender->m_Object;
    PHelper().CreateCaption(items, PrepareKey(pref, "Statistic\\Count"), shared_str().printf("%d", m_pEditObject->SurfaceCount()));
    if (SURF)
    {
        PHelper().CreateCaption(items, PrepareKey(pref, "Surface\\Name"), SURF->_Name());
        xr_string _pref = PrepareKey(pref, "Surface").c_str();
        m_pEditObject->FillSurfaceProps(SURF, _pref.c_str(), items);
    }
}
//------------------------------------------------------------------------------
xr_token eo_type_token[] =
{
    {"Static", 0},
    {"Dynamic", CEditableObject::eoDynamic},
    {"HOM", CEditableObject::eoHOM},
    {"Multiple Usage", CEditableObject::eoMultipleUsage},
    {"Sound Occluder", CEditableObject::eoSoundOccluder},
    {0, 0}
};

void CActorTools::FillObjectProperties(PropItemVec& items, LPCSTR pref, ListItem* sender)
{
    R_ASSERT(m_pEditObject);
    PropValue* V      = 0;
    m_pEditObjectType = m_pEditObject->m_objectFlags.flags & (CEditableObject::eoDynamic | CEditableObject::eoHOM | CEditableObject::eoSoundOccluder | CEditableObject::eoMultipleUsage);
    PHelper().CreateToken32(items, "Object\\Object Type", &m_pEditObjectType, eo_type_token)->OnChangeEvent.bind(this, &CActorTools::OnTypeChange);

    if (m_pEditObjectType & CEditableObject::eoDynamic)
    {
        auto FlagOpt1 = PHelper().CreateFlag32(items, "Object\\Model export\\Optimize:\\Make progressive", &m_pEditObject->m_objectFlags, CEditableObject::eoProgressive);
        FlagOpt1->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
        FlagOpt1->Owner()->hint_text =
            "Make progressive meshes:\n creates progressive meshes when exporting OGF.\n Is are dynamic optimize of the model (lod's),\n is used to optimize world objects."_RU >
            u8"Make progressive meshes:\n создает прогрессивные меши при экспорте OGF.\n Это динамическая детализация модели (lod'ы),\n используется для оптимизации мировых объектов.";

        auto FlagOpt2 = PHelper().CreateFlag32(items, "Object\\Model export\\Optimize:\\Make stripify", &m_pEditObject->m_objectFlags, CEditableObject::eoStripify);
        FlagOpt2->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
        FlagOpt2->Owner()->hint_text =
            "Make stripify meshes:\n optimization of vertex's and face's of meshes, which spoiled the mesh of polygons,\n used to be by default in the SDK and was used to optimize meshes\n for old DirectX and video cards. Can be enabled to optimize world models."_RU >
            u8"Make stripify meshes:\n оптимизация vertex'ов и face'ов у мешей, которая портила сетку полигонов,\n раньше стояла по дефолту в SDK и использовалась для оптимизации мешей\n под старый DirectX и видеокарты. Можно включать для оптимизации мировых моделей.";

        PHelper().CreateFlag32(items, "Object\\Model export\\Optimize:\\Optimize surfaces", &m_pEditObject->m_objectFlags, CEditableObject::eoOptimizeSurf)->Owner()->hint_text =
            "Optimize surfaces:\n combines meshes with the same textures and shaders into one."_RU >
            u8"Optimize surfaces:\n объединяет меши с одинаковыми текстурами и шейдерами в один.";

        auto FlagHQ1 = PHelper().CreateFlag32(items, "Object\\Model export\\Optimize:\\HQ Geometry", &m_pEditObject->m_objectFlags, CEditableObject::eoHQExport);
        FlagHQ1->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
        auto FlagHQ2 = PHelper().CreateFlag32(items, "Object\\Model export\\Optimize:\\HQ Geometry Plus", &m_pEditObject->m_objectFlags,  CEditableObject::eoHQExportPlus);
        FlagHQ2->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
        FlagHQ2->Owner()->hint_text =
            "HQ Geometry+:\n the compiler will not remove similar vertex's and faces'y,\n support for a denser mesh of polygons."_RU >
            u8"HQ Geometry+:\n компилятор не будет удалять похожие vertex'ы и face'ы,\n поддержка более плотной сетки полигонов.";

        PHelper().CreateFlag32(items, "Object\\Model export\\SoC bone export", &m_pEditObject->m_objectFlags, CEditableObject::eoSoCInfluence)->Owner()->hint_text =
            "SoC bone export:\n when exporting a dynamic OGF, a polygon will be affected by a maximum of 2 bones.\n If disabled, CoP influence of 4 bones will be enabled (not supported in SoC)"_RU >
            u8"Экспорт костей SoC:\n при экспорте динамического OGF, на полигон будут влиять максимум 2 кости.\n При отключении будет включено CoP влияние в 4 кости(не поддерживается в SoC).";
    }
    else if (m_pEditObjectType & CEditableObject::eoMultipleUsage)
    {
        PHelper().CreateFlag32(items, "Object\\Flags\\Using LOD", &m_pEditObject->m_objectFlags, CEditableObject::eoUsingLOD)->OnChangeEvent.bind(this, &CActorTools::OnUsingLodFlagChange);
    }

    auto FlagSM1 = PHelper().CreateFlag32(items, "Object\\Model export\\Smooth Type:\\Use split Normals", &m_pEditObject->m_objectFlags, CEditableObject::eoNormals);
    FlagSM1->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
    FlagSM1->Owner()->hint_text =
        "Anti-aliasing type when exporting a model - Normals:\n uses original Split normals, if the model has them.\n Default - when importing a model into the editor\n the anti-aliasing type is determined automatically\n and the necessary anti-aliasing flag is already set."_RU >
        u8"Тип сглаживания при экспорте модели - Normals:\n использует оригинальные Split нормали, если таковые имеются у модели.\n По умолчанию - при импорте модели в редактор\n тип сглаживания определяется автоматически\n и уже установлен необходимый флаг сглаживания.";
    auto FlagSM2 = PHelper().CreateFlag32(items, "Object\\Model export\\Smooth Type:\\Smooth CS/CoP", &m_pEditObject->m_objectFlags, CEditableObject::eoCoPSmooth);
    FlagSM2->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
    FlagSM2->Owner()->hint_text =
        "Anti-aliasing type when exporting a model - CoP: type #2.\n Default - when importing a model into the editor\n the anti-aliasing type is determined automatically\n and the necessary anti-aliasing flag is already set."_RU >
        u8"Тип сглаживания при экспорте модели - CoP: тип #2.\n По умолчанию - при импорте модели в редактор\n тип сглаживания определяется автоматически\n и уже установлен необходимый флаг сглаживания.";
    auto FlagSM3 = PHelper().CreateFlag32(items, "Object\\Model export\\Smooth Type:\\Smooth SoC", &m_pEditObject->m_objectFlags, CEditableObject::eoSoCSmooth);
    FlagSM3->OnChangeEvent.bind(this, &CActorTools::OnChangeFlag);
    FlagSM3->Owner()->hint_text =
        "Anti-aliasing type when exporting a model - SoC: type #1.\n Default - when importing a model into the editor\n the anti-aliasing type is determined automatically\n and the necessary anti-aliasing flag is already set."_RU >
        u8"Тип сглаживания при экспорте модели - SoC: тип #1.\n По умолчанию - при импорте модели в редактор\n тип сглаживания определяется автоматически\n и уже установлен необходимый флаг сглаживания.";

    V = PHelper().CreateVector(items, "Object\\Transform\\Position", &m_pEditObject->a_vPosition, -10000, 10000, 0.01, 2);
    V->OnChangeEvent.bind(this, &CActorTools::OnChangeTransform);
    V = PHelper().CreateAngle3(items, "Object\\Transform\\Rotation", &m_pEditObject->a_vRotate, -10000, 10000, 0.1, 1);
    V->OnChangeEvent.bind(this, &CActorTools::OnChangeTransform);
    V = PHelper().CreateFloat(items, "Object\\Transform\\Scale", &m_pEditObject->a_vScale, -10000, 10000, 0.01, 2);
    V->OnChangeEvent.bind(this, &CActorTools::OnChangeTransform);
    PHelper().CreateBOOL(items, "Object\\Transform\\Adjust Mass By Scale", &m_pEditObject->a_vAdjustMass);
    V = PHelper().CreateCaption( items, "Object\\Transform\\BBox Min", shared_str().printf("{%3.2f, %3.2f, %3.2f}", VPUSH(m_pEditObject->GetBox().min)));
    V = PHelper().CreateCaption(items, "Object\\Transform\\BBox Max", shared_str().printf("{%3.2f, %3.2f, %3.2f}", VPUSH(m_pEditObject->GetBox().max)));

    // PHelper().CreateChoose(items, "Object\\LOD\\Reference", &m_pEditObject->m_LODs, smObject);
    PHelper().CreateChoose(items, "Object\\LOD\\Reference", &m_pEditObject->m_LODs, smVisual);
    if (m_pEditObject->m_objectFlags.flags & CEditableObject::eoUsingLOD)
    {
        PHelper().CreateButton(items, "Object\\LOD\\Action", "Make HQ,Make LQ", ButtonValue::flFirstOnly)->OnBtnClickEvent.bind(this, &CActorTools::OnMakeLODClick);
    }
    PHelper().CreateButton(items, "Object\\Action", "Make Thumbnail", ButtonValue::flFirstOnly)->OnBtnClickEvent.bind(this, &CActorTools::OnMakeThumbnailClick);
    m_pEditObject->FillSummaryProps("Object\\Summary", items);
}
//------------------------------------------------------------------------------

void CActorTools::SelectListItem(LPCSTR pref, LPCSTR name, bool bVal, bool bLeaveSel, bool bExpand)
{
    xr_string nm = (name && name[0]) ? PrepareKey(pref, name).c_str() : xr_string(pref).c_str();
    m_ObjectItems->SelectItem(nm.c_str());
    /*if (pref)
    {
        m_ObjectItems->SelectItem(pref);
    }*/
}
//------------------------------------------------------------------------------

void CActorTools::OnChangeFlag(PropValue* sender)
{
    const auto flag = dynamic_cast<Flag32Value*>(sender);
    //------------------------------------------------------------------------------
    // HQ Geometry / HQ Geometry+
    const bool changingHqGeom      = !strcmp(flag->Owner()->Key(), "Object\\Model export\\Optimize:\\HQ Geometry");
    const auto hqFlag              = CEditableObject::eoHQExport;
    const auto hq2Flag             = CEditableObject::eoHQExportPlus;

    const bool hqSet               = m_pEditObject->m_objectFlags.test(hqFlag);
    const bool hq2Set              = m_pEditObject->m_objectFlags.test(hq2Flag);

    if (hqSet && hq2Set)
    {
        if (changingHqGeom)
            m_pEditObject->m_objectFlags.set(hq2Flag, FALSE);
        else
            m_pEditObject->m_objectFlags.set(hqFlag, FALSE);
    }
    //------------------------------------------------------------------------------
    // Make progressive / Make stripify
    const bool changingProgressive = !strcmp(flag->Owner()->Key(), "Object\\Model export\\Optimize:\\Make progressive");
    const auto ProgFlag            = CEditableObject::eoProgressive;
    const auto Prog2Flag           = CEditableObject::eoStripify;

    const bool ProgSet             = m_pEditObject->m_objectFlags.test(ProgFlag);
    const bool Prog2Set            = m_pEditObject->m_objectFlags.test(Prog2Flag);

    if (ProgSet && Prog2Set)
    {
        if (changingProgressive)
            m_pEditObject->m_objectFlags.set(Prog2Flag, FALSE);
        else
            m_pEditObject->m_objectFlags.set(ProgFlag, FALSE);
    }
    //------------------------------------------------------------------------------
    // split normals / CS/CoP Smooth / SoC Smooth
    const bool changingNormals     = !strcmp(flag->Owner()->Key(), "Object\\Model export\\Smooth Type:\\Use split Normals");
    const bool changingCoP         = !strcmp(flag->Owner()->Key(), "Object\\Model export\\Smooth Type:\\Smooth CS/CoP");
    const bool changingSoC         = !strcmp(flag->Owner()->Key(), "Object\\Model export\\Smooth Type:\\Smooth SoC");
    const auto Smooth1Flag         = CEditableObject::eoNormals;
    const auto Smooth2Flag         = CEditableObject::eoCoPSmooth;
    const auto Smooth3Flag         = CEditableObject::eoSoCSmooth;

    const bool Smooth1Set          = m_pEditObject->m_objectFlags.test(Smooth1Flag);
    const bool Smooth2Set          = m_pEditObject->m_objectFlags.test(Smooth2Flag);
    const bool Smooth3Set          = m_pEditObject->m_objectFlags.test(Smooth3Flag);

    if (Smooth1Set || Smooth2Set || Smooth3Set)
    {
        if (changingNormals)
        {
            m_pEditObject->m_objectFlags.set(Smooth2Flag, FALSE);
            m_pEditObject->m_objectFlags.set(Smooth3Flag, FALSE);
        }
        if (changingCoP)
        {
            m_pEditObject->m_objectFlags.set(Smooth1Flag, FALSE);
            m_pEditObject->m_objectFlags.set(Smooth3Flag, FALSE);
        }
        if (changingSoC)
        {
            m_pEditObject->m_objectFlags.set(Smooth1Flag, FALSE);
            m_pEditObject->m_objectFlags.set(Smooth2Flag, FALSE);
        }
    }
}
