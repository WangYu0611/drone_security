#include "SequenceDispatchPanelWidget.h"
#include "Command/CommandTheme.h"
#include "PathFileListItemWidget.h"
#include "PathDroneMatchItemWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CheckBox.h"
#include "RealTimeDroneReceiver.h"
#include "MultiDroneCharacter.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/DroneOpsTypes.h"
#include "DroneOps/Network/DroneNetworkManager.h"
#include "DroneOps/Control/DroneOpsPlayerController.h"
#include "PathEditor/DronePathActor.h"
#include "PathEditor/DronePlaybackManager.h"
#include "PreviewConfirmPopupWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Blueprint/WidgetTree.h"

namespace
{
UPathDroneMatchItemWidget* GetMatchItemFromScrollChild(UWidget* Child)
{
	if (UPathDroneMatchItemWidget* Item = Cast<UPathDroneMatchItemWidget>(Child))
	{
		return Item;
	}

	if (USizeBox* SizeBox = Cast<USizeBox>(Child))
	{
		return Cast<UPathDroneMatchItemWidget>(SizeBox->GetContent());
	}

	return nullptr;
}
}

bool USequenceDispatchPanelWidget::bStaticPanelInteractive = false;

bool USequenceDispatchPanelWidget::IsPanelInteractive()
{
	return bStaticPanelInteractive;
}

void USequenceDispatchPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OpenButton)
	{
		OpenButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnOpenButtonClicked);
	}
	if (GlobalStopButton)
	{
		GlobalStopButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnGlobalStopClicked);
	}
	if (DispatchButton)
	{
		DispatchButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnDispatchClicked);
	}
	if (LocalPreviewButton)
	{
		LocalPreviewButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnLocalPreviewClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnBackClicked);
	}
	if (AutoAssignCheckBox)
	{
		AutoAssignCheckBox->OnCheckStateChanged.AddDynamic(this, &USequenceDispatchPanelWidget::OnAutoAssignCheckChanged);
		bAutoAssignEnabled = AutoAssignCheckBox->IsChecked();
	}
	if (LocalPatrolSimulationToggle)
	{
		LocalPatrolSimulationToggle->OnCheckStateChanged.AddDynamic(
			this, &USequenceDispatchPanelWidget::OnLocalPatrolSimulationToggleChanged);
	}
	if (PathEditToggle)
	{
		PathEditToggle->OnCheckStateChanged.AddDynamic(this, &USequenceDispatchPanelWidget::OnPathEditToggleChanged);
	}
	if (SavePathButton)
	{
		SavePathButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnSavePathClicked);
	}
	if (PlayEditPathButton)
	{
		PlayEditPathButton->OnClicked.AddDynamic(this, &USequenceDispatchPanelWidget::OnPlayEditPathClicked);
	}
	if (EditLoopCheckBox)
	{
		EditLoopCheckBox->OnCheckStateChanged.AddDynamic(this, &USequenceDispatchPanelWidget::OnEditLoopCheckChanged);
	}
	// 初始隐藏全部编辑相关控件（保存/播放/停止/循环），进入编辑模式再显示。
	SetEditControlsVisible(false);
	PopulateModeComboBox();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDroneNetworkManager* NetMgr = GI->GetSubsystem<UDroneNetworkManager>())
		{
			AssignmentResultHandle = NetMgr->OnAssignmentResult.AddUObject(
				this, &USequenceDispatchPanelWidget::HandleAssignmentResult);
			AssemblyCompleteHandle = NetMgr->OnAssemblyComplete.AddUObject(
				this, &USequenceDispatchPanelWidget::OnAssemblyCompleteForDispatch);
			AssemblyTimeoutHandle = NetMgr->OnAssemblyTimeout.AddUObject(
				this, &USequenceDispatchPanelWidget::OnAssemblyTimeoutForDispatch);
		}
	}

	if (IsStrictLocalPreviewIsolationEnabled())
	{
		if (AutoAssignCheckBox)
		{
			AutoAssignCheckBox->SetIsEnabled(false);
		}
		if (LocalPatrolSimulationToggle)
		{
			LocalPatrolSimulationToggle->SetIsChecked(true);
			LocalPatrolSimulationToggle->SetIsEnabled(false);
		}
		SetStatusMessage(TEXT("纯本地预演模式：后端派发已禁用"));
	}

	SetPanelState(ESequencePanelState::Collapsed);
}

void USequenceDispatchPanelWidget::NativeDestruct()
{
	// 面板销毁：清理编队旋转预览可视化与 Gizmo
	StopFormationRotatePreview();
	EndLocalPatrolSimulation(true);

	// 编辑中被销毁：清理临时路径，避免残留
	if (bInPathEditMode)
	{
		if (ADroneOpsPlayerController* PC = GetDroneOpsController())
		{
			PC->ClearEditingPaths();
		}
		bInPathEditMode = false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDroneNetworkManager* NetMgr = GI->GetSubsystem<UDroneNetworkManager>())
		{
			// 三个句柄各自独立移除，互不依赖，避免订阅模式变化时漏移除造成悬挂订阅。
			if (AssignmentResultHandle.IsValid())
			{
				NetMgr->OnAssignmentResult.Remove(AssignmentResultHandle);
				AssignmentResultHandle.Reset();
			}
			if (AssemblyCompleteHandle.IsValid())
			{
				NetMgr->OnAssemblyComplete.Remove(AssemblyCompleteHandle);
				AssemblyCompleteHandle.Reset();
			}
			if (AssemblyTimeoutHandle.IsValid())
			{
				NetMgr->OnAssemblyTimeout.Remove(AssemblyTimeoutHandle);
				AssemblyTimeoutHandle.Reset();
			}
		}
	}

	Super::NativeDestruct();
}

void USequenceDispatchPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

    // Command owns visibility; never resurrect its collapsed drawer or block the transparent center.
    if (bCommandPresentation) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	const bool bCursorVisible = PC->bShowMouseCursor;
	const ESlateVisibility DesiredVisibility = bCursorVisible
		? ESlateVisibility::Visible
		: ESlateVisibility::HitTestInvisible;

	if (GetVisibility() != DesiredVisibility)
	{
		SetVisibility(DesiredVisibility);
	}
}

void USequenceDispatchPanelWidget::SetCommandPresentation()
{
    bCommandPresentation = true;
    SetColorAndOpacity(FLinearColor::White);
    CommandTheme::Drawer(WidgetTree);
    if (WidgetTree && WidgetTree->RootWidget) WidgetTree->RootWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (OpenButton) OpenButton->SetVisibility(ESlateVisibility::Collapsed);
    if (GlobalStopButton) GlobalStopButton->SetVisibility(ESlateVisibility::Collapsed);
    if (PathEditToggle) PathEditToggle->SetVisibility(ESlateVisibility::Collapsed);
    SetVisibility(ESlateVisibility::Collapsed);
}
void USequenceDispatchPanelWidget::ToggleCommandTools()
{
    if (IsVisible())
    {
        SetPanelState(ESequencePanelState::Collapsed);
        SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (CurrentState == ESequencePanelState::Collapsed) OnOpenButtonClicked();
    }
}

void USequenceDispatchPanelWidget::SetPanelState(ESequencePanelState NewState)
{
	CurrentState = NewState;
	bStaticPanelInteractive = (NewState != ESequencePanelState::Collapsed);
	if (NewState != ESequencePanelState::Matching)
	{
		SelectedPathIndex = INDEX_NONE;
	}

	if (FileListPanel)
	{
		FileListPanel->SetVisibility(NewState == ESequencePanelState::FileList
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MatchingPanel)
	{
		MatchingPanel->SetVisibility(NewState == ESequencePanelState::Matching
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (DispatchButton)
	{
		DispatchButton->SetVisibility(NewState == ESequencePanelState::Matching
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LocalPreviewButton)
	{
		LocalPreviewButton->SetVisibility(NewState == ESequencePanelState::Matching
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BackButton)
	{
		BackButton->SetVisibility(NewState != ESequencePanelState::Collapsed
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USequenceDispatchPanelWidget::OnOpenButtonClicked()
{
	if (CurrentState == ESequencePanelState::Collapsed)
	{
		ScanDronePathFiles();
		SetPanelState(ESequencePanelState::FileList);
	}
	else
	{
		SetPanelState(ESequencePanelState::Collapsed);
	}
}

void USequenceDispatchPanelWidget::OnBackClicked()
{
	// 离开匹配界面：收起编队旋转预览
	StopFormationRotatePreview();

	if (CurrentState == ESequencePanelState::Matching)
	{
		MatchedPairs.Empty();
		SelectedPathIndex = INDEX_NONE;
		SetPanelState(ESequencePanelState::FileList);
		ScanDronePathFiles();
	}
	else
	{
		SetPanelState(ESequencePanelState::Collapsed);
	}
}

void USequenceDispatchPanelWidget::ScanDronePathFiles()
{
	if (!FileListScrollBox) return;
	FileListScrollBox->ClearChildren();

	const FString SearchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DronePaths"));
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(SearchDir, TEXT("*.json")), true, false);

	if (FoundFiles.IsEmpty())
	{
		SetStatusMessage(TEXT("DronePaths 文件夹为空"));
		return;
	}

	SetStatusMessage(FString::Printf(TEXT("找到 %d 个文件"), FoundFiles.Num()));

	for (const FString& FileName : FoundFiles)
	{
		if (!FileListItemClass) continue;

		UPathFileListItemWidget* Item = CreateWidget<UPathFileListItemWidget>(this, FileListItemClass);
		if (Item)
		{
			USizeBox* SizeBox = NewObject<USizeBox>(FileListScrollBox);
			SizeBox->SetMinDesiredHeight(40.f);
			SizeBox->AddChild(Item);
			FileListScrollBox->AddChild(SizeBox);
			Item->SetFileInfo(FileName, FPaths::Combine(SearchDir, FileName));
			Item->OnClicked.BindUObject(this, &USequenceDispatchPanelWidget::OnFileSelected);
		}
	}
}

void USequenceDispatchPanelWidget::OnFileSelected(const FString& FilePath)
{
	StopFormationRotatePreview();

	LoadedPathsData = FDronePathsSaveData();
	if (!UDronePathSaveLibrary::LoadPathsFromJson(FilePath, LoadedPathsData))
	{
		SetStatusMessage(TEXT("加载文件失败"));
		return;
	}

	if (LoadedPathsData.Paths.IsEmpty())
	{
		SetStatusMessage(TEXT("文件中无有效路径"));
		return;
	}

	MatchedPairs.Empty();
	SelectedPathIndex = INDEX_NONE;
	PopulateMatchingView();
	SetPanelState(ESequencePanelState::Matching);

	// 生成预览可视化 + 锚点旋转环 Gizmo：可实时拖动旋转整组路径。
	StartFormationRotatePreview();

	SetStatusMessage(FString::Printf(TEXT("已加载 %d 条路径，先点击 Path，再点击无人机分配（可拖动锚点旋转环旋转编队）"), LoadedPathsData.Paths.Num()));
}

void USequenceDispatchPanelWidget::StartFormationRotatePreview()
{
	if (ADroneOpsPlayerController* PC = GetDroneOpsController())
	{
		if (!PC->BeginFormationRotatePreview(LoadedPathsData))
		{
			SetStatusMessage(TEXT("未找到可用的运行锚点无人机，无法生成旋转预览（请选中一架无人机）"));
		}
	}
}

void USequenceDispatchPanelWidget::StopFormationRotatePreview()
{
	if (ADroneOpsPlayerController* PC = GetDroneOpsController())
	{
		PC->EndFormationRotatePreview();
	}
}

FVector USequenceDispatchPanelWidget::GetSourceAnchorEditOrigin() const
{
	// 手动编辑路径文件的局部原点固定为 JSON 数组中第一条有效路径的首航点。
	// 不使用 AnchorDroneId，避免文件内路径编号或无人机匹配方式改变原点语义。
	const FDronePathSaveData* FirstPath = LoadedPathsData.Paths.FindByPredicate(
		[](const FDronePathSaveData& P) { return P.Waypoints.Num() > 0; });
	return FirstPath ? FirstPath->Waypoints[0].Location : FVector::ZeroVector;
}

FDronePathSaveData USequenceDispatchPanelWidget::BuildTransformedPath(const FDronePathSaveData& SourcePath, const FVector& TargetBase, float YawDegrees) const
{
	// 拷贝原始路径 -> 运行副本；只变换坐标，绝不修改原始 JSON 数据。
	FDronePathSaveData Result = SourcePath;
	const FVector RefEditOrigin = GetSourceAnchorEditOrigin();
	for (FDroneWaypointSaveData& Wp : Result.Waypoints)
	{
		Wp.Location = ADroneOpsPlayerController::ApplyFormationTransform(Wp.Location, RefEditOrigin, TargetBase, YawDegrees);
	}
	return Result;
}

void USequenceDispatchPanelWidget::PopulateMatchingView()
{
	if (PathListScrollBox) PathListScrollBox->ClearChildren();
	if (DroneListScrollBox) DroneListScrollBox->ClearChildren();
	if (!PathListScrollBox || !DroneListScrollBox || !MatchItemClass) return;

	for (int32 i = 0; i < LoadedPathsData.Paths.Num(); ++i)
	{
		UPathDroneMatchItemWidget* PathItem = CreateWidget<UPathDroneMatchItemWidget>(this, MatchItemClass);
		if (PathItem)
		{
			USizeBox* SizeBox = NewObject<USizeBox>(PathListScrollBox);
			SizeBox->SetMinDesiredHeight(44.f);
			SizeBox->AddChild(PathItem);
			if (PathListScrollBox) PathListScrollBox->AddChild(SizeBox);
			PathItem->SetAsPathItem(LoadedPathsData.Paths[i].PathId, i);
			PathItem->OnItemClicked.BindUObject(this, &USequenceDispatchPanelWidget::OnItemClicked);
		}
	}

	UGameInstance* GI = GetOwningPlayer() ? GetOwningPlayer()->GetGameInstance() : nullptr;
	if (!GI) return;

	UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
	if (!Registry) return;

	TArray<FDroneDescriptor> Descriptors = Registry->GetAllDroneDescriptors();
	for (const FDroneDescriptor& Desc : Descriptors)
	{
		UPathDroneMatchItemWidget* DroneItem = CreateWidget<UPathDroneMatchItemWidget>(this, MatchItemClass);
		if (DroneItem)
		{
			USizeBox* SizeBox = NewObject<USizeBox>(DroneListScrollBox);
			SizeBox->SetMinDesiredHeight(44.f);
			SizeBox->AddChild(DroneItem);
			if (DroneListScrollBox) DroneListScrollBox->AddChild(SizeBox);
			DroneItem->SetAsDroneItem(Desc.DroneId, Desc.BackendIdString);
			DroneItem->OnItemClicked.BindUObject(this, &USequenceDispatchPanelWidget::OnItemClicked);
		}
	}

	UpdateMatchVisuals();
	UpdateDispatchButtonState();
}

void USequenceDispatchPanelWidget::PopulateModeComboBox()
{
	if (!ModeComboBox)
	{
		return;
	}

	ModeComboBox->ClearOptions();
	ModeComboBox->AddOption(TEXT("scout"));
	ModeComboBox->AddOption(TEXT("patrol"));
	ModeComboBox->AddOption(TEXT("attack"));
	ModeComboBox->SetSelectedOption(DroneCommandModeToProtocolString(DispatchMode));
	ModeComboBox->OnSelectionChanged.AddDynamic(this, &USequenceDispatchPanelWidget::OnModeSelectionChanged);
}

void USequenceDispatchPanelWidget::OnItemClicked(bool bIsPath, int32 IndexOrId)
{
	if (bAutoAssignEnabled)
	{
		SetStatusMessage(TEXT("自动分配已开启，无需手动匹配（取消勾选可手动指定）"));
		return;
	}

	if (bIsPath)
	{
		if (!LoadedPathsData.Paths.IsValidIndex(IndexOrId))
		{
			return;
		}

		SelectedPathIndex = (SelectedPathIndex == IndexOrId) ? INDEX_NONE : IndexOrId;
		UpdateMatchVisuals();

		if (SelectedPathIndex == INDEX_NONE)
		{
			SetStatusMessage(TEXT("已取消选择 Path"));
			return;
		}

		const int32 PathId = LoadedPathsData.Paths[SelectedPathIndex].PathId;
		SetStatusMessage(FString::Printf(TEXT("已选择 Path %d，请点击无人机分配"), PathId));
		return;
	}

	if (!LoadedPathsData.Paths.IsValidIndex(SelectedPathIndex))
	{
		SetStatusMessage(TEXT("请先点击一个 Path，再点击无人机分配"));
		return;
	}

	const int32 PathIndex = SelectedPathIndex;
	const int32 DroneId = IndexOrId;

	for (auto It = MatchedPairs.CreateIterator(); It; ++It)
	{
		if (It->Value == DroneId)
		{
			It.RemoveCurrent();
			break;
		}
	}

	MatchedPairs.Add(PathIndex, DroneId);
	SelectedPathIndex = INDEX_NONE;

	UpdateMatchVisuals();
	UpdateDispatchButtonState();
	SetStatusMessage(FString::Printf(TEXT("已匹配 %d / %d"), MatchedPairs.Num(), LoadedPathsData.Paths.Num()));
}

void USequenceDispatchPanelWidget::UpdateMatchVisuals()
{
	if (PathListScrollBox)
	{
		for (int32 i = 0; i < PathListScrollBox->GetChildrenCount(); ++i)
		{
			UPathDroneMatchItemWidget* Item = GetMatchItemFromScrollChild(PathListScrollBox->GetChildAt(i));
			if (!Item) continue;
			if (const int32* Matched = MatchedPairs.Find(Item->GetPathIndex()))
			{
				Item->SetMatchedLabel(FString::Printf(TEXT("-> d%d"), *Matched));
			}
			else
			{
				Item->ClearMatch();
			}

			if (Item->GetPathIndex() == SelectedPathIndex)
			{
				Item->SetSelected(true);
			}
		}
	}

	if (DroneListScrollBox)
	{
		for (int32 i = 0; i < DroneListScrollBox->GetChildrenCount(); ++i)
		{
			UPathDroneMatchItemWidget* Item = GetMatchItemFromScrollChild(DroneListScrollBox->GetChildAt(i));
			if (!Item) continue;
			bool bMatched = false;
			for (const auto& Pair : MatchedPairs)
			{
				if (Pair.Value == Item->GetItemId())
				{
					Item->SetMatchedLabel(FString::Printf(TEXT("<- Path %d"), LoadedPathsData.Paths[Pair.Key].PathId));
					bMatched = true;
					break;
				}
			}
			if (!bMatched) Item->ClearMatch();
		}
	}
}

void USequenceDispatchPanelWidget::UpdateDispatchButtonState()
{
	if (!DispatchButton) return;
	if (const ADroneOpsPlayerController* OpsController = GetDroneOpsController();
		OpsController && OpsController->IsVideoKeyboardControlActive())
	{
		DispatchButton->SetIsEnabled(false);
		return;
	}
	if (IsStrictLocalPreviewIsolationEnabled())
	{
		DispatchButton->SetIsEnabled(false);
		return;
	}
	const bool bAllMatched = MatchedPairs.Num() == LoadedPathsData.Paths.Num() && !LoadedPathsData.Paths.IsEmpty();
	const bool bAutoReady = bAutoAssignEnabled && !LoadedPathsData.Paths.IsEmpty();
	DispatchButton->SetIsEnabled(bAllMatched || bAutoReady);
}

void USequenceDispatchPanelWidget::OnAutoAssignCheckChanged(bool bIsChecked)
{
	bAutoAssignEnabled = bIsChecked;
	SelectedPathIndex = INDEX_NONE;

	if (bIsChecked)
	{
		MatchedPairs.Empty();
		SetStatusMessage(TEXT("自动分配已开启：派发后由后端匈牙利算法分配无人机"));
	}
	else
	{
		SetStatusMessage(TEXT("自动分配已关闭：请手动匹配路径与无人机"));
	}

	UpdateMatchVisuals();
	UpdateDispatchButtonState();
}

void USequenceDispatchPanelWidget::OnDispatchClicked()
{
	if (ADroneOpsPlayerController* PC = GetDroneOpsController(); PC && PC->IsVideoKeyboardControlActive())
	{
		SetStatusMessage(TEXT("视频键盘操控期间不支持队列播放"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Dispatch] OnDispatchClicked entered: matched=%d paths=%d autoAssign=%d"),
		MatchedPairs.Num(), LoadedPathsData.Paths.Num(), bAutoAssignEnabled ? 1 : 0);

	// 新一轮派发开始：复位集结早到兜底标志，避免上一批的完成信号误放行本批。
	bAssemblyReleasePending = false;

	if (!bAutoAssignEnabled && MatchedPairs.Num() != LoadedPathsData.Paths.Num())
	{
		SetStatusMessage(TEXT("请先完成所有路径匹配"));
		return;
	}

	if (LoadedPathsData.Paths.IsEmpty())
	{
		SetStatusMessage(TEXT("请先加载路径文件"));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	UGameInstance* GI = PC->GetGameInstance();
	if (!GI) return;

	UDroneNetworkManager* NetMgr = GI->GetSubsystem<UDroneNetworkManager>();
	if (!NetMgr) return;

	if (!NetMgr->CanSendToBackend())
	{
		SetStatusMessage(UDroneNetworkManager::GetIsolationBlockedMessage());
		return;
	}

	if (bAutoAssignEnabled)
	{
		UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
		if (!Registry)
		{
			return;
		}
		DispatchWithAutoAssign(NetMgr, Registry);
		return;
	}

	// Build DroneId -> PathSaveData map
	TMap<int32, FDronePathSaveData> DispatchMap;
	for (const auto& Pair : MatchedPairs)
	{
		if (LoadedPathsData.Paths.IsValidIndex(Pair.Key))
		{
			DispatchMap.Add(Pair.Value, LoadedPathsData.Paths[Pair.Key]);
		}
	}

	// 派发瞬间重新读取运行锚点，不能使用打开文件时缓存的位置；否则无人机在
	// 面板打开后移动过，首航点仍可能落回旧位置。ResolveRunAnchorDrone 会在
	// 多选时取编号最小的已选无人机，单选时取主选中无人机。
	ADroneOpsPlayerController* OpsPC = GetDroneOpsController();
	if (!OpsPC)
	{
		SetStatusMessage(TEXT("路径控制器不可用，无法确定路径原点"));
		return;
	}

	int32 RefDroneId = INDEX_NONE;
	FVector RefAnchor = FVector::ZeroVector;
	if (!OpsPC->ResolveRunAnchorDrone(RefDroneId, RefAnchor))
	{
		SetStatusMessage(TEXT("请先选中一架具有影子机的无人机，无法确定路径原点"));
		return;
	}

	float Yaw = 0.0f;
	if (OpsPC->IsFormationRotateActive())
	{
		Yaw = OpsPC->GetFormationYawDegrees();
	}

	// 路径 JSON 的首航点是局部原点，不是需要飞回的上电坐标。
	// 无论真机或本地预演，都把它放到选定基准影子机的当前位置；其余航点
	// 保持相对首点的偏移。这样诸如 (0,0,0) 起点的方形路径会从当前高度开始。

	// Remap all waypoints: 以源锚点为几何参考做水平旋转，再平移到当前运行锚点。
	// 与本地预演使用相同的影子机当前位置作为基准，原始 JSON 不会被修改。
	// 全部作用于运行副本，不修改原始 JSON。
	TMap<int32, FDronePathSaveData> RemappedMap;
	for (auto& Pair : DispatchMap)
	{
		RemappedMap.Add(Pair.Key, BuildTransformedPath(Pair.Value, RefAnchor, Yaw));
	}

	// RemappedMap is Cesium absolute world space and must remain that way for shadow playback.
	// PX4, however, consumes a power-on-origin-relative NED target.  Build a separate wire copy
	// through the controller's geographic conversion so every path uses the anchor of its assigned
	// drone.  Sending RemappedMap directly here would make the backend interpret Cesium world
	// coordinates as local offsets, which can put a real drone hundreds of metres off course.
	TMap<int32, FDronePathSaveData> BackendDispatchMap;
	FString PathConvertError;
	if (!OpsPC || !OpsPC->TryBuildBackendRelativePathData(
		RemappedMap, BackendDispatchMap, PathConvertError))
	{
		SetStatusMessage(PathConvertError.IsEmpty()
			? TEXT("队列路径坐标转换失败，未发送任何派发请求")
			: PathConvertError);
		return;
	}

	// 派发前收起旋转环：已派发的路径不再允许拖动旋转，避免目标突然跳变。
	StopFormationRotatePreview();

	FOnHttpResponse Callback;
	Callback.BindDynamic(this, &USequenceDispatchPanelWidget::OnDispatchResponse);
	PendingRemappedMap = RemappedMap;
	NetMgr->SendArrayTaskFromData(BackendDispatchMap, DispatchMode, Callback);

	SetStatusMessage(FString::Printf(TEXT("正在派发... 参考机DroneId=%d  mode=%s"),
		RefDroneId, *DroneCommandModeToProtocolString(DispatchMode)));
	if (DispatchButton) DispatchButton->SetIsEnabled(false);
}

void USequenceDispatchPanelWidget::DispatchWithAutoAssign(UDroneNetworkManager* NetMgr, UDroneRegistrySubsystem* Registry)
{
	// A real aircraft's route must be converted against the anchor of the aircraft that will fly it.
	// Backend auto-assignment chooses that aircraft after receiving the request, so it has no safe
	// way to derive per-aircraft local offsets from these Cesium world-space paths.  Keep mock/offline
	// auto-assignment available, but require an explicit manual match whenever a real anchored drone
	// is online rather than risking an offset route.
	for (const FDroneDescriptor& Desc : Registry->GetAllDroneDescriptors())
	{
		const ARealTimeDroneReceiver* Receiver = Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(Desc.DroneId));
		FDroneTelemetrySnapshot Snapshot;
		if (Receiver && Receiver->bHasGpsAnchor
			&& Registry->GetTelemetry(Desc.DroneId, Snapshot)
			&& Snapshot.Availability == EDroneAvailability::Online)
		{
			SetStatusMessage(TEXT("真机队列请手动匹配路径与无人机；自动分配无法安全处理每架机独立GPS锚点"));
			return;
		}
	}

	// 参考锚点：编号最小且已有 GPS 锚点的无人机（与手动模式的参考机规则一致）
	int32 RefDroneId = MAX_int32;
	FVector RefAnchor = FVector::ZeroVector;
	bool bHasAnchor = false;
	for (const FDroneDescriptor& Desc : Registry->GetAllDroneDescriptors())
	{
		if (Desc.DroneId >= RefDroneId) continue;
		if (AActor* ReceiverActor = Registry->GetReceiverActor(Desc.DroneId))
		{
			if (ARealTimeDroneReceiver* Receiver = Cast<ARealTimeDroneReceiver>(ReceiverActor))
			{
				if (Receiver->bHasGpsAnchor)
				{
					RefDroneId = Desc.DroneId;
					RefAnchor = Receiver->AnchorWorldLocation;
					bHasAnchor = true;
				}
			}
		}
	}

	if (!bHasAnchor)
	{
		SetStatusMessage(TEXT("没有无人机获得GPS锚点，无法自动分配派发"));
		return;
	}

	// 当前编队旋转角（与本地预演一致）；无预览时按 0 度处理（纯平移）。
	float Yaw = 0.0f;
	if (ADroneOpsPlayerController* PC = GetDroneOpsController())
	{
		if (PC->IsFormationRotateActive())
		{
			Yaw = PC->GetFormationYawDegrees();
		}
	}

	// PathId -> 运行副本：以源锚点为几何参考旋转，再整体平移到参考机 GPS 锚点；drone_id 由后端分配。
	// 与手动派发/本地预演使用完全一致的旋转 + 源锚点原点，仅平移基准不同。不修改原始 JSON。
	TMap<int32, FDronePathSaveData> RemappedByPathId;
	for (const FDronePathSaveData& Path : LoadedPathsData.Paths)
	{
		RemappedByPathId.Add(Path.PathId, BuildTransformedPath(Path, RefAnchor, Yaw));
	}

	// 派发前收起旋转环。
	StopFormationRotatePreview();

	FOnHttpResponse Callback;
	Callback.BindDynamic(this, &USequenceDispatchPanelWidget::OnDispatchResponse);
	PendingRemappedMap.Empty();
	PendingAutoPathsByPathId = RemappedByPathId;
	NetMgr->SendArrayTaskFromData(RemappedByPathId, DispatchMode, Callback, /*bAutoAssign=*/true);

	SetStatusMessage(FString::Printf(TEXT("正在派发（自动分配）... 参考锚点DroneId=%d  mode=%s"),
		RefDroneId, *DroneCommandModeToProtocolString(DispatchMode)));
	if (DispatchButton) DispatchButton->SetIsEnabled(false);
}

void USequenceDispatchPanelWidget::HandleAssignmentResult(const FString& ArrayId, const TArray<FDronePathAssignment>& Assignments)
{
	if (PendingAutoPathsByPathId.IsEmpty())
	{
		return; // 不是本面板发起的自动分配任务
	}

	// 按分配结果把 PathId -> PathData 转成 DroneId -> PathData，并同步槽位显示
	TMap<int32, FDronePathSaveData> ByDrone;
	MatchedPairs.Empty();
	for (const FDronePathAssignment& Assignment : Assignments)
	{
		const FDronePathSaveData* Path = PendingAutoPathsByPathId.Find(Assignment.PathId);
		if (!Path) continue;

		ByDrone.Add(Assignment.DroneId, *Path);

		for (int32 i = 0; i < LoadedPathsData.Paths.Num(); ++i)
		{
			if (LoadedPathsData.Paths[i].PathId == Assignment.PathId)
			{
				MatchedPairs.Add(i, Assignment.DroneId);
				break;
			}
		}
	}
	PendingAutoPathsByPathId.Empty();

	UpdateMatchVisuals();
	SetStatusMessage(FString::Printf(TEXT("自动分配完成（%s）：%d 条路径已分配"), *ArrayId, ByDrone.Num()));

	UE_LOG(LogTemp, Log, TEXT("[SequenceDispatch] Assignment result applied: array=%s, %d paths"),
		*ArrayId, ByDrone.Num());

	// 按分配结果驱动影子机本地播放
	PendingRemappedMap = ByDrone;
	StartShadowDronePlayback();
	UpdateDispatchButtonState();
}

void USequenceDispatchPanelWidget::OnModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	DispatchMode = DroneCommandModeFromProtocolString(SelectedItem);
}

void USequenceDispatchPanelWidget::OnLocalPatrolSimulationToggleChanged(bool bIsChecked)
{
	if (!bIsChecked)
	{
		EndLocalPatrolSimulation();
	}
}

void USequenceDispatchPanelWidget::OnDispatchResponse(bool bSuccess, const FString& ResponseBody)
{
	if (bSuccess)
	{
		if (bAutoAssignEnabled)
		{
			// 面板保持打开，等待 assignment_result 推送后同步槽位显示并启动影子机
			SetStatusMessage(TEXT("派发成功，等待后端自动分配结果..."));
		}
		else
		{
			SetStatusMessage(TEXT("派发成功"));
			StartShadowDronePlayback();
			MatchedPairs.Empty();
			SetPanelState(ESequencePanelState::Collapsed);
		}
	}
	else
	{
		SetStatusMessage(ResponseBody.Contains(TEXT("assembly already in progress"))
			? TEXT("派发失败：后端已有阵列任务，请先点击“停止”，待后端停止成功后再派发")
			: FString::Printf(TEXT("派发失败: %s"), *ResponseBody));
		PendingRemappedMap.Empty();
		PendingAutoPathsByPathId.Empty();
		UpdateDispatchButtonState();
	}
}

void USequenceDispatchPanelWidget::SetStatusMessage(const FString& Message)
{
	if (StatusText)
	{
		if (bCommandPresentation)
		{
			StatusText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			StatusText->SetToolTipText(FText::FromString(Message));
		}
		StatusText->SetText(FText::FromString(Message));
	}
}

void USequenceDispatchPanelWidget::OnLocalPreviewClicked()
{
	if (MatchedPairs.Num() != LoadedPathsData.Paths.Num())
	{
		SetStatusMessage(TEXT("请先完成所有路径匹配"));
		return;
	}

	// 每次点击预演都重读影子机当前位置，确保 JSON 首点始终是当前局部原点。
	ADroneOpsPlayerController* PC = GetDroneOpsController();
	int32 RunAnchorDroneId = INDEX_NONE;
	FVector RunAnchor = FVector::ZeroVector;
	if (!PC || !PC->ResolveRunAnchorDrone(RunAnchorDroneId, RunAnchor))
	{
		SetStatusMessage(TEXT("无有效运行锚点，无法预演（请先选中一架无人机）"));
		return;
	}
	const float Yaw = PC->IsFormationRotateActive() ? PC->GetFormationYawDegrees() : 0.0f;

	// 不发送后端请求，直接用匹配结果驱动影子机本地播放。
	// 应用与派发完全一致的旋转 + 平移，仅作用于运行副本，不修改原始 JSON。
	TMap<int32, FDronePathSaveData> PreviewMap;
	for (const auto& Pair : MatchedPairs)
	{
		if (LoadedPathsData.Paths.IsValidIndex(Pair.Key))
		{
			PreviewMap.Add(Pair.Value, BuildTransformedPath(LoadedPathsData.Paths[Pair.Key], RunAnchor, Yaw));
		}
	}

	// 预演开始前收起旋转环，避免执行中继续拖动导致目标跳变。
	StopFormationRotatePreview();

	PendingRemappedMap = PreviewMap;
	bLocalPreviewPlaybackRequested = true;
	StartShadowDronePlayback();
	if (LastLocalPreviewStartedPathCount == 0)
	{
		SetStatusMessage(FString::Printf(TEXT("本地预演未启动：%d 条路径不足 2 个航点，%d 条路径未找到影子机"),
			LastLocalPreviewSkippedInsufficientWaypointCount,
			LastLocalPreviewSkippedNoShadowCount));
		return;
	}
	BeginLocalPatrolSimulation(PreviewMap);

	if (IsLocalPatrolSimulationEnabled() && DispatchMode == EDroneCommandMode::Patrol)
	{
		SetStatusMessage(FString::Printf(TEXT("本地巡逻模拟已开始：%d 条路径启动（未向后端发送指令）"),
			LastLocalPreviewStartedPathCount));
	}
	else
	{
		SetStatusMessage(FString::Printf(TEXT("本地预演已开始：%d 条路径启动（未向后端发送指令）"),
			LastLocalPreviewStartedPathCount));
	}
	MatchedPairs.Empty();
	SetPanelState(ESequencePanelState::Collapsed);
}

void USequenceDispatchPanelWidget::ClearActiveDispatchPaths()
{
	EndLocalPreviewPlayback();
	EndLocalPatrolSimulation(true);

	for (ADronePathActor* PathActor : ActiveDispatchPathActors)
	{
		if (IsValid(PathActor))
		{
			PathActor->StopMovement();
			PathActor->Destroy();
		}
	}
	ActiveDispatchPathActors.Empty();
}

void USequenceDispatchPanelWidget::EndLocalPreviewPlayback()
{
	if (LocalPreviewPlaybackDroneIds.IsEmpty())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
	if (Registry)
	{
		for (int32 DroneId : LocalPreviewPlaybackDroneIds)
		{
			if (AMultiDroneCharacter* Shadow = Cast<AMultiDroneCharacter>(Registry->GetSenderPawn(DroneId)))
			{
				Shadow->SetLocalPathPreviewActive(false);
			}
		}
	}

	LocalPreviewPlaybackDroneIds.Empty();
}

void USequenceDispatchPanelWidget::OnGlobalStopClicked()
{
	// 1. 全局停止并销毁所有 manager 与路径 Actor（含卡在循环中的 JSON 播放）。
	ADronePlaybackManager::StopAndClearAllInWorld(GetWorld());

	// 2. 本面板派发路径 Actor 已被上面销毁，仅清引用。
	ActiveDispatchPathActors.Empty();
	EndLocalPreviewPlayback();
	EndLocalPatrolSimulation(true);

	// 3. 若在编辑模式：清理临时路径并复位编辑状态与 UI。
	if (bInPathEditMode)
	{
		if (ADroneOpsPlayerController* PC = GetDroneOpsController())
		{
			PC->EndPathEditMode();
			PC->ClearEditingPaths();
		}
		ResetPathEditState();
	}

	// 4. 清空待发送缓存与待启动缓存。
	PendingRemappedMap.Empty();
	bLocalPreviewPlaybackRequested = false;
	PendingArrivalPaths.Empty();
	bAssemblyReleasePending = false;

	// 5. The backend AssemblyController is independent from local path actors and otherwise
	// remains busy after an old task. Release it as part of the same explicit global-stop action.
	UGameInstance* GI = GetGameInstance();
	UDroneNetworkManager* NetMgr = GI ? GI->GetSubsystem<UDroneNetworkManager>() : nullptr;
	if (NetMgr && NetMgr->CanSendToBackend())
	{
		FOnHttpResponse Callback;
		Callback.BindDynamic(this, &USequenceDispatchPanelWidget::OnBackendArrayStopResponse);
		NetMgr->StopActiveArrayTask(Callback);
		SetStatusMessage(TEXT("本地路径已清除，正在停止后端阵列任务..."));
	}
	else if (NetMgr)
	{
		SetStatusMessage(TEXT("本地路径已清除；严格本地隔离已开启，未向后端发送停止请求"));
	}
	else
	{
		SetStatusMessage(TEXT("本地路径已清除；网络管理器不可用，无法停止后端阵列任务"));
	}
}

void USequenceDispatchPanelWidget::OnBackendArrayStopResponse(bool bSuccess, const FString& ResponseBody)
{
	if (bSuccess)
	{
		SetStatusMessage(TEXT("已停止后端阵列任务并清除所有本地路径"));
		return;
	}

	SetStatusMessage(ResponseBody.IsEmpty()
		? TEXT("本地路径已清除，但后端阵列停止失败：后端无响应")
		: FString::Printf(TEXT("本地路径已清除，但后端阵列停止失败：%s"), *ResponseBody));
}

void USequenceDispatchPanelWidget::OnDispatchPathExecutionStateChanged(ADronePathActor* PathActor, EDronePathExecutionState NewState)
{
	// 仅在路径播放完成时自动清除（循环路径永不 Completed，保留至全局停止）。
	if (NewState != EDronePathExecutionState::Completed || !IsValid(PathActor))
	{
		return;
	}

	ActiveDispatchPathActors.Remove(PathActor);
	PathActor->StopMovement();
	PathActor->Destroy();

	if (ActiveDispatchPathActors.IsEmpty())
	{
		EndLocalPreviewPlayback();
		EndLocalPatrolSimulation();
	}
}

bool USequenceDispatchPanelWidget::IsLocalPatrolSimulationEnabled() const
{
	return IsStrictLocalPreviewIsolationEnabled() ||
		(LocalPatrolSimulationToggle && LocalPatrolSimulationToggle->IsChecked());
}

bool USequenceDispatchPanelWidget::IsStrictLocalPreviewIsolationEnabled() const
{
	UGameInstance* GI = GetGameInstance();
	const UDroneNetworkManager* NetMgr =
		GI ? GI->GetSubsystem<UDroneNetworkManager>() : nullptr;
	return NetMgr && NetMgr->IsStrictLocalPreviewIsolation();
}

void USequenceDispatchPanelWidget::BeginLocalPatrolSimulation(const TMap<int32, FDronePathSaveData>& PreviewMap)
{
	if (DispatchMode != EDroneCommandMode::Patrol || !IsLocalPatrolSimulationEnabled())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LocalPatrolSimulation] Drone registry unavailable"));
		return;
	}

	for (const TPair<int32, FDronePathSaveData>& Pair : PreviewMap)
	{
		if (Pair.Key <= 0 || Pair.Value.Waypoints.IsEmpty())
		{
			continue;
		}

		Registry->UpdateLocalState(Pair.Key, EUELocalDroneState::LocalPatrolling);
		LocalPatrolSimulationDroneIds.Add(Pair.Key);
	}

	UE_LOG(LogTemp, Log, TEXT("[LocalPatrolSimulation] Enabled for %d local preview drone(s)"),
		LocalPatrolSimulationDroneIds.Num());
}

void USequenceDispatchPanelWidget::EndLocalPatrolSimulation(bool bForceClearAllLocalStates)
{
	if (LocalPatrolSimulationDroneIds.IsEmpty())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
	if (Registry)
	{
		for (int32 DroneId : LocalPatrolSimulationDroneIds)
		{
			FDroneTelemetrySnapshot Snapshot;
			if (!Registry->GetTelemetry(DroneId, Snapshot))
			{
				continue;
			}

			const bool bShouldClear = bForceClearAllLocalStates ||
				Snapshot.LocalState == EUELocalDroneState::LocalPatrolling;
			if (bShouldClear)
			{
				Registry->UpdateLocalState(DroneId, EUELocalDroneState::None);
			}
		}
	}

	LocalPatrolSimulationDroneIds.Empty();
}

bool USequenceDispatchPanelWidget::IsOnlineRealDrone(int32 DroneId) const
{
	UGameInstance* GI = GetGameInstance();
	UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
	if (!Registry)
	{
		return false;
	}

	FDroneTelemetrySnapshot Snap;
	if (!Registry->GetTelemetry(DroneId, Snap) || Snap.Availability != EDroneAvailability::Online)
	{
		return false;
	}

	// 镜像机存在且已拿到 GPS 锚点，才有真实位置可供影子机跟随等待。
	if (ARealTimeDroneReceiver* Receiver = Cast<ARealTimeDroneReceiver>(Registry->GetReceiverActor(DroneId)))
	{
		return Receiver->bHasGpsAnchor;
	}
	return false;
}

void USequenceDispatchPanelWidget::StartPendingArrivalPaths()
{
	for (const FPendingArrivalPath& Pending : PendingArrivalPaths)
	{
		ADronePathActor* PathActor = Pending.PathActor.Get();
		APawn* ShadowPawn = Pending.ShadowPawn.Get();
		if (!IsValid(PathActor) || !IsValid(ShadowPawn))
		{
			continue;
		}

		if (AMultiDroneCharacter* Shadow = Cast<AMultiDroneCharacter>(ShadowPawn))
		{
			Shadow->bFollowingMirror = false;
		}
		PathActor->StartMovement(ShadowPawn);
	}
	PendingArrivalPaths.Empty();
	bAssemblyReleasePending = false;
}

void USequenceDispatchPanelWidget::OnAssemblyCompleteForDispatch(const FString& ArrayId)
{
	if (PendingArrivalPaths.IsEmpty())
	{
		// 事件早于路径入队到达：记下完成信号，待 StartShadowDronePlayback 入队后立即放行。
		bAssemblyReleasePending = true;
		return;
	}
	StartPendingArrivalPaths();
	SetStatusMessage(TEXT("集结完成，真机已到位，开始沿路径播放"));
}

void USequenceDispatchPanelWidget::OnAssemblyTimeoutForDispatch(const FString& ArrayId, int32 ReadyCount, int32 TotalCount)
{
	// 兜底：集结超时也放行，避免路径永久卡在等待。
	if (PendingArrivalPaths.IsEmpty())
	{
		// 事件早于路径入队到达：记下放行信号，待入队后立即放行。
		bAssemblyReleasePending = true;
		return;
	}
	StartPendingArrivalPaths();
	SetStatusMessage(FString::Printf(TEXT("集结超时（%d/%d），仍启动路径播放"), ReadyCount, TotalCount));
}

void USequenceDispatchPanelWidget::StartShadowDronePlayback()
{
	// Consume the one-shot request flag immediately. Any early return below must not leak a
	// local-preview state into a later formal-dispatch callback.
	const bool bThisPlaybackIsLocalPreview =
		bLocalPreviewPlaybackRequested || IsStrictLocalPreviewIsolationEnabled();
	bLocalPreviewPlaybackRequested = false;
	LastLocalPreviewStartedPathCount = 0;
	LastLocalPreviewSkippedNoShadowCount = 0;
	LastLocalPreviewSkippedInsufficientWaypointCount = 0;

	ClearActiveDispatchPaths();

	// 清除上一批遗留的待启动项，避免新旧混放（ClearActiveDispatchPaths 已销毁其路径 Actor）。
	PendingArrivalPaths.Empty();

	if (PendingRemappedMap.IsEmpty())
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	UWorld* World = PC->GetWorld();
	if (!World) return;

	UGameInstance* GI = PC->GetGameInstance();
	if (!GI) return;

	UDroneRegistrySubsystem* Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
	if (!Registry) return;

	for (const TPair<int32, FDronePathSaveData>& Pair : PendingRemappedMap)
	{
		const int32 DroneId = Pair.Key;
		const FDronePathSaveData& PathData = Pair.Value;

		if (PathData.Waypoints.Num() < 2)
		{
			if (bThisPlaybackIsLocalPreview)
			{
				++LastLocalPreviewSkippedInsufficientWaypointCount;
			}
			UE_LOG(LogTemp, Warning, TEXT("[SequenceDispatch] DroneId=%d path %d skipped: playback requires at least 2 waypoints"),
				DroneId, PathData.PathId);
			continue;
		}

		// 找到对应影子机（SenderPawn）
		APawn* ShadowPawn = Registry->GetSenderPawn(DroneId);
		if (!IsValid(ShadowPawn))
		{
			if (bThisPlaybackIsLocalPreview)
			{
				++LastLocalPreviewSkippedNoShadowCount;
			}
			UE_LOG(LogTemp, Warning, TEXT("[SequenceDispatch] DroneId=%d path %d skipped: no shadow pawn is registered"),
				DroneId, PathData.PathId);
			continue;
		}

		// Local preview is an entirely local execution path. Do not let stale/real-time
		// Online telemetry put the shadow path into the assembly_complete wait queue.
		const bool bWaitForArrival = !bThisPlaybackIsLocalPreview && IsOnlineRealDrone(DroneId);

		// 离线 / mock：立即停止跟随镜像机；在线真机：保持跟随，等集结完成再解除。
		if (!bWaitForArrival)
		{
			if (AMultiDroneCharacter* Shadow = Cast<AMultiDroneCharacter>(ShadowPawn))
			{
				if (bThisPlaybackIsLocalPreview)
				{
					Shadow->SetLocalPathPreviewActive(true);
					LocalPreviewPlaybackDroneIds.Add(DroneId);
				}
				else
				{
					Shadow->bFollowingMirror = false;
				}
			}
		}

		// Spawn DronePathActor
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADronePathActor* PathActor = World->SpawnActor<ADronePathActor>(
			ADronePathActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!IsValid(PathActor)) continue;

		PathActor->SetPathNumericId(PathData.PathId);
		PathActor->bClosedLoop = PathData.bClosedLoop;

		// segmentSpeed=0 表示"使用默认速度"，DronePathActor 需要 >0 的速度才能正常计算 duration
		const float DefaultSpeedMps = ADronePathActor::GetDefaultSegmentSpeedMps();

		for (int32 i = 0; i < PathData.Waypoints.Num(); ++i)
		{
			const FDroneWaypointSaveData& WpData = PathData.Waypoints[i];
			const float EffectiveSpeed = (i == 0) ? 0.0f : (WpData.SegmentSpeed > KINDA_SMALL_NUMBER ? WpData.SegmentSpeed : DefaultSpeedMps);
			const int32 NewIdx = PathActor->AddWaypoint(WpData.Location, EffectiveSpeed);
			if (PathActor->Waypoints.IsValidIndex(NewIdx))
			{
				PathActor->Waypoints[NewIdx].WaitTime = WpData.WaitTime;
				PathActor->Waypoints[NewIdx].SegmentSpeed = EffectiveSpeed;
			}
		}

		PathActor->RefreshPath();
		PathActor->OnExecutionStateChanged.AddUObject(
			this, &USequenceDispatchPanelWidget::OnDispatchPathExecutionStateChanged);

		ActiveDispatchPathActors.Add(PathActor);

		if (bWaitForArrival)
		{
			// 在线真机：先不启动，等 assembly_complete 放行。
			FPendingArrivalPath Pending;
			Pending.PathActor = PathActor;
			Pending.ShadowPawn = ShadowPawn;
			PendingArrivalPaths.Add(Pending);

			UE_LOG(LogTemp, Log, TEXT("[SequenceDispatch] Online drone DroneId=%d path %d spawned, waiting for assembly_complete"),
				DroneId, PathData.PathId);
		}
		else
		{
			PathActor->StartMovement(ShadowPawn);
			if (bThisPlaybackIsLocalPreview)
			{
				++LastLocalPreviewStartedPathCount;
			}
			UE_LOG(LogTemp, Log, TEXT("[SequenceDispatch] Shadow drone DroneId=%d started playback along path %d"),
				DroneId, PathData.PathId);
		}
	}

	PendingRemappedMap.Empty();

	// 兜底：若集结完成/超时事件在入队前已到达，此处立即放行等待中的在线真机路径。
	if (bAssemblyReleasePending && !PendingArrivalPaths.IsEmpty())
	{
		StartPendingArrivalPaths();
		SetStatusMessage(TEXT("集结已完成（事件早到），开始沿路径播放"));
	}

	if (bThisPlaybackIsLocalPreview)
	{
		if (LastLocalPreviewStartedPathCount == 0)
		{
			EndLocalPreviewPlayback();
		}

		UE_LOG(LogTemp, Log, TEXT("[SequenceDispatch] Local preview summary: started=%d, skippedNoShadow=%d, skippedInsufficientWaypoints=%d"),
			LastLocalPreviewStartedPathCount,
			LastLocalPreviewSkippedNoShadowCount,
			LastLocalPreviewSkippedInsufficientWaypointCount);
	}
}

// ================= 路径编辑模式 =================

ADroneOpsPlayerController* USequenceDispatchPanelWidget::GetDroneOpsController() const
{
	return Cast<ADroneOpsPlayerController>(GetOwningPlayer());
}

void USequenceDispatchPanelWidget::OnPathEditToggleChanged(bool bIsChecked)
{
	if (bIsChecked)
	{
		if (ADroneOpsPlayerController* PC = GetDroneOpsController(); PC && PC->IsVideoKeyboardControlActive())
		{
			if (PathEditToggle) PathEditToggle->SetIsChecked(false);
			SetStatusMessage(TEXT("视频键盘操控期间不支持路径点编辑"));
			return;
		}
	}
	if (bIsChecked)
	{
		// 收集当前选中无人机：优先多选，否则主选
		TArray<int32> DroneIds;
		UGameInstance* GI = GetGameInstance();
		UDroneRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UDroneRegistrySubsystem>() : nullptr;
		if (Registry)
		{
			DroneIds = Registry->GetMultiSelectedDrones();
			if (DroneIds.IsEmpty())
			{
				const int32 Primary = Registry->GetPrimarySelectedDrone();
				if (Primary > 0)
				{
					DroneIds.Add(Primary);
				}
			}
		}

		// 不再要求先选中无人机：可空手进入，进入后点击场景中的无人机动态增删。
		ADroneOpsPlayerController* PC = GetDroneOpsController();
		if (!PC || !PC->BeginPathEditMode(DroneIds))
		{
			SetStatusMessage(TEXT("无法进入编辑模式"));
			// 复位 Toggle（避免递归：SetIsChecked 不触发 OnCheckStateChanged）
			if (PathEditToggle)
			{
				PathEditToggle->SetIsChecked(false);
			}
			return;
		}

		bInPathEditMode = true;
		// 进入编辑模式：显示保存/播放/停止/循环控件，循环框复位为未勾选。
		SetEditControlsVisible(true);
		if (EditLoopCheckBox)
		{
			EditLoopCheckBox->SetIsChecked(false);
		}
		SetStatusMessage(DroneIds.IsEmpty()
			? FString(TEXT("编辑模式：点击无人机加入编辑，点地图加航点，拖拽 gizmo 微调"))
			: FString::Printf(TEXT("编辑模式：%d 架无人机，点击无人机可增减，点地图加航点，拖拽 gizmo 微调"), DroneIds.Num()));
	}
	else
	{
		if (!bInPathEditMode)
		{
			return;
		}

		// 取消编辑模式 = 直接退出并清空（等同三选弹窗的"取消"）：
		// 销毁临时 ADronePathActor、航点 Actor 与缓存数据，场景不残留。
		bInPathEditMode = false;

		if (ADroneOpsPlayerController* PC = GetDroneOpsController())
		{
			PC->EndPathEditMode();
			PC->ClearEditingPaths();
		}

		ResetPathEditState();
		SetStatusMessage(TEXT("已退出编辑模式，临时路径已清理"));
	}
}

bool USequenceDispatchPanelWidget::BeginCommandPlanning()
{
    if (bInPathEditMode) return true;
    OnPathEditToggleChanged(true);
    if (PathEditToggle) PathEditToggle->SetIsChecked(bInPathEditMode);
    return bInPathEditMode;
}

bool USequenceDispatchPanelWidget::ConfirmCommandPath()
{
    ADroneOpsPlayerController* PC = GetDroneOpsController();
    if (!PC || !bInPathEditMode || PC->IsVideoKeyboardControlActive() || !PreviewConfirmPopupClass) return false;
    const auto Paths = PC->BuildEditingPathsData();
    if (Paths.IsEmpty()) return false;
    for (const auto& Pair : Paths) if (Pair.Value.Waypoints.Num() < 2) return false;
    OnPlayEditPathClicked();
    return true;
}

bool USequenceDispatchPanelWidget::CancelCommandPlanning()
{
    if (!bInPathEditMode) return false;
    OnPathEditToggleChanged(false);
    if (PathEditToggle) PathEditToggle->SetIsChecked(false);
    return true;
}

void USequenceDispatchPanelWidget::OnSavePathClicked()
{
	ADroneOpsPlayerController* PC = GetDroneOpsController();
	if (!PC)
	{
		return;
	}

	SavedPreviewData = PC->BuildEditingPathsData();
	bHasSavedPreviewData = !SavedPreviewData.IsEmpty();
	SetStatusMessage(bHasSavedPreviewData
		? FString::Printf(TEXT("已保存 %d 条路径到内存"), SavedPreviewData.Num())
		: TEXT("没有可保存的航点"));
}

void USequenceDispatchPanelWidget::OnPlayEditPathClicked()
{
	if (ADroneOpsPlayerController* PC = GetDroneOpsController(); PC && PC->IsVideoKeyboardControlActive())
	{
		SetStatusMessage(TEXT("视频键盘操控期间不支持队列播放"));
		return;
	}
	if (!bInPathEditMode)
	{
		return;
	}

	// 播放：弹出三选弹窗（预演/派发/取消），沿用现有逻辑。
	ShowPreviewConfirmPopup();
}

void USequenceDispatchPanelWidget::OnEditLoopCheckChanged(bool bIsChecked)
{
	// 只作用于临时路径的运行副本：同步设置所有临时 ADronePathActor 的循环状态。
	if (ADroneOpsPlayerController* PC = GetDroneOpsController())
	{
		PC->SetAllEditingPathsClosedLoop(bIsChecked);
	}
	SetStatusMessage(bIsChecked ? TEXT("临时路径：循环播放") : TEXT("临时路径：单次播放"));
}

void USequenceDispatchPanelWidget::SetEditControlsVisible(bool bVisible)
{
	const ESlateVisibility Vis = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (SavePathButton)
	{
		SavePathButton->SetVisibility(Vis);
	}
	if (PlayEditPathButton)
	{
		PlayEditPathButton->SetVisibility(Vis);
	}
	if (EditLoopCheckBox)
	{
		EditLoopCheckBox->SetVisibility(Vis);
	}
}

TMap<int32, FDronePathSaveData> USequenceDispatchPanelWidget::ResolvePreviewDispatchMap()
{
	// Editing may continue after "save to memory" (map click, gizmo drag, or geographic batch add).
	// While edit mode is active, always rebuild from the live path actors so preview and final dispatch
	// cannot consume a stale snapshot that omits newly appended waypoints or their current order.
	if (bInPathEditMode)
	{
		if (ADroneOpsPlayerController* PC = GetDroneOpsController())
		{
			return PC->BuildEditingPathsData();
		}
	}

	if (bHasSavedPreviewData && !SavedPreviewData.IsEmpty())
	{
		return SavedPreviewData;
	}

	return TMap<int32, FDronePathSaveData>();
}

void USequenceDispatchPanelWidget::ShowPreviewConfirmPopup()
{
	if (!PreviewConfirmPopupClass)
	{
		// 没配置弹窗类：仅提示，保留编辑模式与临时路径（用停止/取消编辑来清理）。
		SetStatusMessage(TEXT("未配置确认弹窗，请在面板蓝图设置 PreviewConfirmPopupClass"));
		return;
	}

	UPreviewConfirmPopupWidget* Popup = CreateWidget<UPreviewConfirmPopupWidget>(GetOwningPlayer(), PreviewConfirmPopupClass);
	if (!Popup)
	{
		return;
	}

	if (IsStrictLocalPreviewIsolationEnabled())
	{
		Popup->SetDispatchAvailable(false,
			TEXT("纯本地预演模式：仅可本地播放，后端指令派发已禁用"));
	}
	Popup->OnChoiceMade.AddDynamic(this, &USequenceDispatchPanelWidget::OnPreviewConfirmChoice);
	Popup->AddToViewport(100);
}

void USequenceDispatchPanelWidget::OnPreviewConfirmChoice(EPreviewConfirmChoice Choice)
{
	switch (Choice)
	{
	case EPreviewConfirmChoice::LocalPreview:
	{
		// 播放（本地预演）：保留临时路径与编辑模式，可继续调整/再播放/停止。
		TMap<int32, FDronePathSaveData> Map = ResolvePreviewDispatchMap();
		if (Map.IsEmpty())
		{
			SetStatusMessage(TEXT("没有可预演的路径"));
			break;
		}
		PendingRemappedMap = Map;
		bLocalPreviewPlaybackRequested = true;
		StartShadowDronePlayback();
		if (LastLocalPreviewStartedPathCount == 0)
		{
			SetStatusMessage(FString::Printf(TEXT("本地预演未启动：%d 条路径不足 2 个航点，%d 条路径未找到影子机"),
				LastLocalPreviewSkippedInsufficientWaypointCount,
				LastLocalPreviewSkippedNoShadowCount));
			break;
		}
		BeginLocalPatrolSimulation(Map);
		if (IsLocalPatrolSimulationEnabled() && DispatchMode == EDroneCommandMode::Patrol)
		{
			SetStatusMessage(FString::Printf(TEXT("本地巡逻模拟已开始：%d 条路径启动（未向后端发送指令，点停止结束）"),
				LastLocalPreviewStartedPathCount));
		}
		else
		{
			SetStatusMessage(FString::Printf(TEXT("本地预演已开始：%d 条路径启动（未向后端发送指令，点停止结束）"),
				LastLocalPreviewStartedPathCount));
		}
		break;
	}
	case EPreviewConfirmChoice::Dispatch:
	{
		TMap<int32, FDronePathSaveData> Map = ResolvePreviewDispatchMap();
		if (Map.IsEmpty())
		{
			SetStatusMessage(TEXT("没有可派发的路径"));
			break;
		}

		UGameInstance* GI = GetGameInstance();
		UDroneNetworkManager* NetMgr = GI ? GI->GetSubsystem<UDroneNetworkManager>() : nullptr;
		if (!NetMgr)
		{
			SetStatusMessage(TEXT("网络管理器不可用，派发失败"));
			break;
		}

		if (!NetMgr->CanSendToBackend())
		{
			SetStatusMessage(UDroneNetworkManager::GetIsolationBlockedMessage());
			break;
		}

		// Editing paths stay in Cesium world space for local playback. Convert a separate
		// wire copy through WGS84 + fresh per-drone local NED so the backend receives
		// power-on-origin-relative UE offsets, without coupling Cesium world axes into PX4.
		TMap<int32, FDronePathSaveData> BackendDispatchMap;
		FString PathConvertError;
		ADroneOpsPlayerController* OpsController = GetDroneOpsController();
		if (!OpsController || !OpsController->TryBuildBackendRelativePathData(
			Map, BackendDispatchMap, PathConvertError))
		{
			SetStatusMessage(PathConvertError.IsEmpty()
				? TEXT("编辑路径坐标转换失败，未发送任何派发请求")
				: PathConvertError);
			break;
		}

		FOnHttpResponse Callback;
		Callback.BindDynamic(this, &USequenceDispatchPanelWidget::OnEditDispatchResponse);
		PendingRemappedMap = Map;
		bLocalPreviewPlaybackRequested = false;
		NetMgr->SendArrayTaskFromData(BackendDispatchMap, DispatchMode, Callback, /*bAutoAssign=*/false);
		SetStatusMessage(FString::Printf(TEXT("正在派发 %d 条路径... mode=%s"),
			Map.Num(), *DroneCommandModeToProtocolString(DispatchMode)));
		break;
	}
	case EPreviewConfirmChoice::Cancel:
	default:
	{
		// 弹窗内取消：什么都不做，停留在编辑模式（临时路径保留）。
		SetStatusMessage(TEXT("已取消播放，继续编辑"));
		break;
	}
	}
}

void USequenceDispatchPanelWidget::ResetPathEditState()
{
	bInPathEditMode = false;
	bHasSavedPreviewData = false;
	SavedPreviewData.Empty();
	// 隐藏全部编辑控件（保存/播放/停止/循环）。
	SetEditControlsVisible(false);
	if (EditLoopCheckBox)
	{
		EditLoopCheckBox->SetIsChecked(false);
	}
	if (PathEditToggle && PathEditToggle->IsChecked())
	{
		PathEditToggle->SetIsChecked(false);
	}
}

void USequenceDispatchPanelWidget::OnEditDispatchResponse(bool bSuccess, const FString& ResponseBody)
{
	if (bSuccess)
	{
		SetStatusMessage(TEXT("派发成功，开始本地预演"));
		StartShadowDronePlayback();
	}
	else
	{
		SetStatusMessage(ResponseBody.Contains(TEXT("assembly already in progress"))
			? TEXT("派发失败：后端已有阵列任务，请先点击“停止”，待后端停止成功后再派发")
			: FString::Printf(TEXT("派发失败: %s"), *ResponseBody));
		PendingRemappedMap.Empty();
	}
}
