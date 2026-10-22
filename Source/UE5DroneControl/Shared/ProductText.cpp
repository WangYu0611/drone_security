#include "Shared/ProductText.h"
namespace {
const TMap<FString,FText>& Catalog(){static const TMap<FString,FText> C={
{TEXT("Plan.Title"),NSLOCTEXT("DroneOps","Plan.Title","SECURITY PLAN")},
{TEXT("Plan.Name"),NSLOCTEXT("DroneOps","Plan.Name","Plan name")},
{TEXT("Plan.Description"),NSLOCTEXT("DroneOps","Plan.Description","Description")},
{TEXT("Plan.New"),NSLOCTEXT("DroneOps","Plan.New","NEW PLAN")},
{TEXT("Plan.Update"),NSLOCTEXT("DroneOps","Plan.Update","SAVE PLAN INFORMATION")},
{TEXT("Plan.Mission"),NSLOCTEXT("DroneOps","Plan.Mission","Task")},
{TEXT("Plan.Missions"),NSLOCTEXT("DroneOps","Plan.Missions","MISSIONS")},
{TEXT("Plan.MissionName"),NSLOCTEXT("DroneOps","Plan.MissionName","Task name")},
{TEXT("Plan.AddMission"),NSLOCTEXT("DroneOps","Plan.AddMission","ADD TASK")},
{TEXT("Plan.Rename"),NSLOCTEXT("DroneOps","Plan.Rename","RENAME TASK")},
{TEXT("Plan.Delete"),NSLOCTEXT("DroneOps","Plan.Delete","DELETE MISSION")},
{TEXT("Plan.Assign"),NSLOCTEXT("DroneOps","Plan.Assign","ASSIGN UAV")},
{TEXT("Plan.Unassign"),NSLOCTEXT("DroneOps","Plan.Unassign","UNASSIGN UAV")},
{TEXT("Plan.NoMissions"),NSLOCTEXT("DroneOps","Plan.NoMissions","No missions / Add first mission")},
{TEXT("Plan.NoPlan"),NSLOCTEXT("DroneOps","Plan.NoPlan","No active plan")},
{TEXT("Plan.EditMap"),NSLOCTEXT("DroneOps","Plan.EditMap","EDIT ROUTE ON MAP")},
{TEXT("Plan.Check"),NSLOCTEXT("DroneOps","Plan.Check","CHECK CONFIGURATION")},
{TEXT("Plan.Review"),NSLOCTEXT("DroneOps","Plan.Review","REVIEW PLAN")},
{TEXT("Plan.ConfirmReview"),NSLOCTEXT("DroneOps","Plan.ConfirmReview","CONFIRM REVIEW")},
{TEXT("Plan.Deploy"),NSLOCTEXT("DroneOps","Plan.Deploy","DEPLOY PLAN")},
{TEXT("Plan.ConfirmDeploy"),NSLOCTEXT("DroneOps","Plan.ConfirmDeploy","CONFIRM DEPLOY")},
{TEXT("Plan.Copy"),NSLOCTEXT("DroneOps","Plan.Copy","COPY AS DRAFT")},
{TEXT("Plan.DRAFT"),NSLOCTEXT("DroneOps","Plan.DRAFT","DRAFT")},
{TEXT("Plan.READY"),NSLOCTEXT("DroneOps","Plan.READY","CONFIGURATION VALID")},
{TEXT("Plan.DEPLOYED"),NSLOCTEXT("DroneOps","Plan.DEPLOYED","Deployed")},
{TEXT("Plan.ReviewNotice"),NSLOCTEXT("DroneOps","Plan.ReviewNotice","This review confirms the current plan configuration. Deployment freezes the business configuration and does not start aircraft execution.")},
{TEXT("Plan.Revision"),NSLOCTEXT("DroneOps","Plan.Revision","Revision {0}")},
{TEXT("Plan.Reviewed"),NSLOCTEXT("DroneOps","Plan.Reviewed","Reviewed revision {0}")},
{TEXT("Plan.MissionSummary"),NSLOCTEXT("DroneOps","Plan.MissionSummary","{0} | UAV: {1} | Waypoints: {2}")},
{TEXT("Plan.Counts"),NSLOCTEXT("DroneOps","Plan.Counts","Tasks: {0} | UAV assigned: {1} | Routes configured: {2}")},
{TEXT("Plan.RouteParameters"),NSLOCTEXT("DroneOps","Plan.RouteParameters","Closed route: {0} | Execution: once | Speed: {1} | Wait: {2}")},
{TEXT("Plan.Blocking"),NSLOCTEXT("DroneOps","Plan.Blocking","Blocking issues")},
{TEXT("Plan.Warnings"),NSLOCTEXT("DroneOps","Plan.Warnings","Configuration check only. Terrain, airspace, endurance and flight safety are not assessed.")},
{TEXT("Plan.Deployment"),NSLOCTEXT("DroneOps","Plan.Deployment","Deployment record")},
{TEXT("Common.Save"),NSLOCTEXT("DroneOps","Common.Save","SAVE")},
{TEXT("Common.Cancel"),NSLOCTEXT("DroneOps","Common.Cancel","CANCEL")},
{TEXT("Common.Discard"),NSLOCTEXT("DroneOps","Common.Discard","DISCARD CHANGES")},
{TEXT("Common.Exit"),NSLOCTEXT("DroneOps","Common.Exit","EXIT")},
{TEXT("Common.None"),NSLOCTEXT("DroneOps","Common.None","Not assigned")},
{TEXT("Common.Waiting"),NSLOCTEXT("DroneOps","Common.Waiting","Waiting for Backend confirmation...")},
{TEXT("Common.Confirmed"),NSLOCTEXT("DroneOps","Common.Confirmed","Backend confirmed")},
{TEXT("Common.Language"),NSLOCTEXT("DroneOps","Common.Language","Language")},
{TEXT("Map.Route"),NSLOCTEXT("DroneOps","Map.Route","MISSION ROUTE")},
{TEXT("Map.Begin"),NSLOCTEXT("DroneOps","Map.Begin","BEGIN ROUTE EDIT")},
{TEXT("Map.Add"),NSLOCTEXT("DroneOps","Map.Add","ADD WAYPOINT")},
{TEXT("Map.Delete"),NSLOCTEXT("DroneOps","Map.Delete","DELETE WAYPOINT")},
{TEXT("Map.Clear"),NSLOCTEXT("DroneOps","Map.Clear","CLEAR ROUTE")},
{TEXT("Map.Focus"),NSLOCTEXT("DroneOps","Map.Focus","FOCUS ROUTE")},
{TEXT("Map.DirtyPrompt"),NSLOCTEXT("DroneOps","Map.DirtyPrompt","Unsaved route changes. Save, discard, or cancel before leaving.")},
{TEXT("Map.DiscardPrompt"),NSLOCTEXT("DroneOps","Map.DiscardPrompt","Discard local changes and reload the saved route?")},
{TEXT("Map.EDITING"),NSLOCTEXT("DroneOps","Map.EDITING","EDITING")},
{TEXT("Map.DIRTY"),NSLOCTEXT("DroneOps","Map.DIRTY","DIRTY / UNSAVED")},
{TEXT("Map.SAVING"),NSLOCTEXT("DroneOps","Map.SAVING","SAVING")},
{TEXT("Map.CLEAN"),NSLOCTEXT("DroneOps","Map.CLEAN","SAVED / READ ONLY")},
{TEXT("Map.Count"),NSLOCTEXT("DroneOps","Map.Count","Waypoints: {0}")},
{TEXT("Video.Open"),NSLOCTEXT("DroneOps","Video.Open","OPEN VIDEO")},
{TEXT("Video.Active"),NSLOCTEXT("DroneOps","Video.Active","VIEW ACTIVE UAV VIDEO")},
{TEXT("Video.Assigned"),NSLOCTEXT("DroneOps","Video.Assigned","VIEW ASSIGNED UAV VIDEO")},
{TEXT("Video.Target"),NSLOCTEXT("DroneOps","Video.Target","VIDEO TARGET")},
{TEXT("Errors.SYNC_OFFLINE"),NSLOCTEXT("DroneOps","Errors.SYNC_OFFLINE","Backend offline. No change submitted.")},
{TEXT("Errors.SYNC_FAILED"),NSLOCTEXT("DroneOps","Errors.SYNC_FAILED","Synchronization failed. Last confirmed state retained.")},
{TEXT("Errors.PLAN_EDIT_IN_PROGRESS"),NSLOCTEXT("DroneOps","Errors.PLAN_EDIT_IN_PROGRESS","Map route editing is in progress. Save or discard before review or deployment.")},
{TEXT("Errors.PLAN_REVIEW_REQUIRED"),NSLOCTEXT("DroneOps","Errors.PLAN_REVIEW_REQUIRED","Review the current configuration first.")},
{TEXT("Errors.REVIEW_STALE"),NSLOCTEXT("DroneOps","Errors.REVIEW_STALE","Review is stale. Review the current revision again.")},
{TEXT("Errors.VALIDATION_STALE"),NSLOCTEXT("DroneOps","Errors.VALIDATION_STALE","Configuration validation is stale.")},
{TEXT("Errors.VERSION_CONFLICT"),NSLOCTEXT("DroneOps","Errors.VERSION_CONFLICT","Version conflict. Local draft retained. Discard and reload to resolve.")},
{TEXT("Errors.EDIT_SESSION_CONFLICT"),NSLOCTEXT("DroneOps","Errors.EDIT_SESSION_CONFLICT","Another editor owns this route session.")},
{TEXT("Errors.EDIT_SESSION_EXPIRED"),NSLOCTEXT("DroneOps","Errors.EDIT_SESSION_EXPIRED","Editing lease expired. Local draft retained; discard and reload.")},
{TEXT("Errors.MAP_CLIENT_UNAVAILABLE"),NSLOCTEXT("DroneOps","Errors.MAP_CLIENT_UNAVAILABLE","Map client unavailable")},
{TEXT("Errors.PLAN_DEPLOYED"),NSLOCTEXT("DroneOps","Errors.PLAN_DEPLOYED","Deployed plan is read only.")},
{TEXT("Errors.PLAN_NOT_READY"),NSLOCTEXT("DroneOps","Errors.PLAN_NOT_READY","Check configuration and resolve blocking issues.")},
{TEXT("Errors.NO_MISSIONS"),NSLOCTEXT("DroneOps","Errors.NO_MISSIONS","Add at least one mission.")},
{TEXT("Errors.NO_UAV_ASSIGNED"),NSLOCTEXT("DroneOps","Errors.NO_UAV_ASSIGNED","Assign a UAV to this mission.")},
{TEXT("Errors.INVALID_UAV"),NSLOCTEXT("DroneOps","Errors.INVALID_UAV","The assigned UAV is not in the Registry.")},
{TEXT("Errors.UAV_ALREADY_ASSIGNED"),NSLOCTEXT("DroneOps","Errors.UAV_ALREADY_ASSIGNED","This UAV is assigned to another mission in this plan.")},
{TEXT("Errors.ROUTE_MISSING"),NSLOCTEXT("DroneOps","Errors.ROUTE_MISSING","Edit and save a route on Map.")},
{TEXT("Errors.ROUTE_TOO_SHORT"),NSLOCTEXT("DroneOps","Errors.ROUTE_TOO_SHORT","Route requires at least 2 waypoints.")},
{TEXT("Errors.INVALID_WAYPOINT"),NSLOCTEXT("DroneOps","Errors.INVALID_WAYPOINT","Waypoint coordinates or parameters are invalid.")},
{TEXT("Errors.INVALID_NAME"),NSLOCTEXT("DroneOps","Errors.INVALID_NAME","Enter a valid name.")},
{TEXT("Errors.INVALID_LANGUAGE"),NSLOCTEXT("DroneOps","Errors.INVALID_LANGUAGE","Unsupported language.")},
{TEXT("Errors.INVALID_VIDEO_TARGET"),NSLOCTEXT("DroneOps","Errors.INVALID_VIDEO_TARGET","Select a registered UAV video target.")},
{TEXT("Errors.Request"),NSLOCTEXT("DroneOps","Errors.Request","Request failed. Confirm the current selection and retry.")},
{TEXT("Video.Header"),NSLOCTEXT("DroneOps","Video.Header","{0} | VIDEO TARGET: {1} | ACTIVE UAV: {2}")},
{TEXT("Video.Feed"),NSLOCTEXT("DroneOps","Video.Feed","{0}\nModel: N/A | Link: {1}\n{2}\n{3}")},
{TEXT("Video.Heading"),NSLOCTEXT("DroneOps","Video.Heading","PRIMARY VIDEO / {0}")},
{TEXT("Video.Details"),NSLOCTEXT("DroneOps","Video.Details","{0} | Camera: N/A | Resolution: {1} | FPS: {2} | {3}")},
{TEXT("Command.Header"),NSLOCTEXT("DroneOps","Command.Header","COMMAND CENTER | {0} | {1} | Clients {2}/3")},
{TEXT("Command.Count"),NSLOCTEXT("DroneOps","Command.Count","{0} UAV | {1} ONLINE | {2} ALERTS")},
{TEXT("Command.Metrics"),NSLOCTEXT("DroneOps","Command.Metrics","ALT {0} | SPD {1} | BAT {2}")},
{TEXT("Command.Link"),NSLOCTEXT("DroneOps","Command.Link","LINK {0}\nMISSION {1}")},
{TEXT("Log.Count"),NSLOCTEXT("DroneOps","Log.Count","EVENT / OPERATION LOG (UTC) | Critical {0} | Warning {1}")},
{TEXT("Map.Header"),NSLOCTEXT("DroneOps","Map.Header","MAP OPERATIONS")},
{TEXT("Map.Init"),NSLOCTEXT("DroneOps","Map.Init","Map initializing")},
{TEXT("Map.Select"),NSLOCTEXT("DroneOps","Map.Select","Select a UAV here or on the map.")},
{TEXT("Map.Locate"),NSLOCTEXT("DroneOps","Map.Locate","LOCATE")},
{TEXT("Video.Title"),NSLOCTEXT("DroneOps","Video.Title","UAV VIDEO MONITOR CENTER")},
{TEXT("Video.Aircraft"),NSLOCTEXT("DroneOps","Video.Aircraft","AIRCRAFT")},
{TEXT("Video.SelectFeed"),NSLOCTEXT("DroneOps","Video.SelectFeed","SELECT VIDEO TARGET")},
{TEXT("Video.ModelNotice"),NSLOCTEXT("DroneOps","Video.ModelNotice","Model not supplied by Registry. Visual presets are not aircraft specifications.")},
{TEXT("Video.Primary"),NSLOCTEXT("DroneOps","Video.Primary","PRIMARY VIDEO")},
{TEXT("Video.NoTarget"),NSLOCTEXT("DroneOps","Video.NoTarget","NO TARGET\nNO ACTIVE STREAM")},
{TEXT("Video.Camera"),NSLOCTEXT("DroneOps","Video.Camera","Camera: N/A")},
{TEXT("Video.AI"),NSLOCTEXT("DroneOps","Video.AI","AI DETECTION")},
{TEXT("Video.AINotConnected"),NSLOCTEXT("DroneOps","Video.AINotConnected","AI DETECTION NOT CONNECTED")},
{TEXT("Video.Categories"),NSLOCTEXT("DroneOps","Video.Categories","PERSON\n\nVEHICLE\n\nUAV\n\nBEHAVIOR\n\nOTHER")},
{TEXT("Video.RecentAlerts"),NSLOCTEXT("DroneOps","Video.RecentAlerts","RECENT ALERTS")},
{TEXT("Video.NoAlerts"),NSLOCTEXT("DroneOps","Video.NoAlerts","NO ACTIVE ALERTS")},
{TEXT("Video.ReservedFields"),NSLOCTEXT("DroneOps","Video.ReservedFields","Reserved fields\nTitle · UAV · Target\nTimestamp · Severity")},
{TEXT("Video.ReservedStatus"),NSLOCTEXT("DroneOps","Video.ReservedStatus","UAV ONLINE: N/A\nAI ENGINE: NOT CONNECTED\nSTORAGE: N/A\nNETWORK: N/A")},
{TEXT("Common.Reserved"),NSLOCTEXT("DroneOps","Common.Reserved","Reserved for future integration")},
{TEXT("Video.Four"),NSLOCTEXT("DroneOps","Video.Four","4 VIEW")},
{TEXT("Video.Six"),NSLOCTEXT("DroneOps","Video.Six","6 VIEW")},
{TEXT("Video.Focus"),NSLOCTEXT("DroneOps","Video.Focus","FOCUS")},
{TEXT("Video.SetActive"),NSLOCTEXT("DroneOps","Video.SetActive","SET AS ACTIVE UAV")},
{TEXT("Video.Retry"),NSLOCTEXT("DroneOps","Video.Retry","RETRY")},
{TEXT("Video.Single"),NSLOCTEXT("DroneOps","Video.Single","SINGLE ACTIVE PLAYER · Secondary feeds are reserved slots · No AI backend connected")},
{TEXT("Video.Reserved"),NSLOCTEXT("DroneOps","Video.Reserved","RESERVED\nNO UAV ASSIGNED")},
{TEXT("Video.NoSource"),NSLOCTEXT("DroneOps","Video.NoSource","NO SOURCE\nAssign a video source to this UAV")},
{TEXT("Command.Title"),NSLOCTEXT("DroneOps","Command.Title","SECURITY COMMAND CENTER")},
{TEXT("Command.Fleet"),NSLOCTEXT("DroneOps","Command.Fleet","UAV FLEET")},
{TEXT("Command.Waiting"),NSLOCTEXT("DroneOps","Command.Waiting","Waiting for Registry")},
{TEXT("Command.Alerts"),NSLOCTEXT("DroneOps","Command.Alerts","ALERTS · Last 200 / Session")},
{TEXT("Command.Tactical"),NSLOCTEXT("DroneOps","Command.Tactical","TACTICAL DATA")},
{TEXT("Command.NoSelection"),NSLOCTEXT("DroneOps","Command.NoSelection","No UAV selected")},
{TEXT("Command.NoMetrics"),NSLOCTEXT("DroneOps","Command.NoMetrics","ALT N/A | SPD N/A | BAT N/A")},
{TEXT("Command.NoMission"),NSLOCTEXT("DroneOps","Command.NoMission","LINK N/A | MISSION N/A")},
{TEXT("Command.ActionNotice"),NSLOCTEXT("DroneOps","Command.ActionNotice","Request results require Backend confirmation. Use Map for route editing.")},
{TEXT("Command.Details"),NSLOCTEXT("DroneOps","Command.Details","UAV DETAILS / ACTIONS")},
{TEXT("Command.Pause"),NSLOCTEXT("DroneOps","Command.Pause","REQUEST PAUSE")},
{TEXT("Command.Resume"),NSLOCTEXT("DroneOps","Command.Resume","REQUEST RESUME")},
{TEXT("Command.NoAlerts"),NSLOCTEXT("DroneOps","Command.NoAlerts","No alerts in this session")},
{TEXT("Command.Select"),NSLOCTEXT("DroneOps","Command.Select","SELECT UAV")},
{TEXT("Command.Handled"),NSLOCTEXT("DroneOps","Command.Handled","MARK HANDLED")},
{TEXT("Command.Remove"),NSLOCTEXT("DroneOps","Command.Remove","REMOVE RECORD")},
{TEXT("Log.Clear"),NSLOCTEXT("DroneOps","Log.Clear","CLEAR DISPLAY")},
{TEXT("Log.Empty"),NSLOCTEXT("DroneOps","Log.Empty","No events in this view")},
{TEXT("Common.Online"),NSLOCTEXT("DroneOps","Common.Online","ONLINE")},
{TEXT("Common.Lost"),NSLOCTEXT("DroneOps","Common.Lost","LOST")},
{TEXT("Common.Offline"),NSLOCTEXT("DroneOps","Common.Offline","OFFLINE")},
{TEXT("Common.Selected"),NSLOCTEXT("DroneOps","Common.Selected","SELECTED")},
{TEXT("Common.Select"),NSLOCTEXT("DroneOps","Common.Select","SELECT")},
{TEXT("Common.Expand"),NSLOCTEXT("DroneOps","Common.Expand","EXPAND")},
{TEXT("Common.Collapse"),NSLOCTEXT("DroneOps","Common.Collapse","COLLAPSE")},
{TEXT("Common.Rename"),NSLOCTEXT("DroneOps","Common.Rename","RENAME")},
{TEXT("Status.RESERVED"),NSLOCTEXT("DroneOps","Status.RESERVED","RESERVED")},
{TEXT("Status.INFO"),NSLOCTEXT("DroneOps","Status.INFO","INFO")},
{TEXT("Status.WARNING"),NSLOCTEXT("DroneOps","Status.WARNING","WARNING")},
{TEXT("Status.CRITICAL"),NSLOCTEXT("DroneOps","Status.CRITICAL","CRITICAL")},
{TEXT("Status.RECORD"),NSLOCTEXT("DroneOps","Status.RECORD","RECORD")},
{TEXT("Status.PLAYBACK"),NSLOCTEXT("DroneOps","Status.PLAYBACK","PLAYBACK")},
{TEXT("Status.SCREENSHOT"),NSLOCTEXT("DroneOps","Status.SCREENSHOT","SCREENSHOT")},
{TEXT("Status.ALERT_FILTER"),NSLOCTEXT("DroneOps","Status.ALERT_FILTER","ALERT FILTER")},
{TEXT("Status.NO_TARGET"),NSLOCTEXT("DroneOps","Status.NO_TARGET","NO TARGET")},
{TEXT("Status.NO_SOURCE"),NSLOCTEXT("DroneOps","Status.NO_SOURCE","NO SOURCE")},
{TEXT("Status.CONNECTING"),NSLOCTEXT("DroneOps","Status.CONNECTING","CONNECTING")},
{TEXT("Status.PAGE_READY___WAITING_VIDEO"),NSLOCTEXT("DroneOps","Status.PAGE_READY___WAITING_VIDEO","PAGE READY / WAITING VIDEO")},
{TEXT("Status.PLAYING"),NSLOCTEXT("DroneOps","Status.PLAYING","PLAYING")},
{TEXT("Status.STALLED___NO_FRAMES"),NSLOCTEXT("DroneOps","Status.STALLED___NO_FRAMES","STALLED / NO FRAMES")},
{TEXT("Status.OFFLINE___LOAD_FAILED"),NSLOCTEXT("DroneOps","Status.OFFLINE___LOAD_FAILED","OFFLINE / LOAD FAILED")},
{TEXT("Status.MEDIA_ERROR"),NSLOCTEXT("DroneOps","Status.MEDIA_ERROR","MEDIA ERROR")},
{TEXT("Status.RETRYING"),NSLOCTEXT("DroneOps","Status.RETRYING","RETRYING")},
{TEXT("Status.CLOSED"),NSLOCTEXT("DroneOps","Status.CLOSED","CLOSED")},
{TEXT("Status.SOURCE_SET"),NSLOCTEXT("DroneOps","Status.SOURCE_SET","SOURCE SET")},
{TEXT("Status.VIDEO_SLOT_NO_ACTIVE_STREAM"),NSLOCTEXT("DroneOps","Status.VIDEO_SLOT_NO_ACTIVE_STREAM","VIDEO SLOT\nNO ACTIVE STREAM")},
{TEXT("Status.MONITOR"),NSLOCTEXT("DroneOps","Status.MONITOR","MONITOR")},
{TEXT("Status.PLAN_EDIT"),NSLOCTEXT("DroneOps","Status.PLAN_EDIT","PLAN_EDIT")},
{TEXT("Status.ALERT_RESPONSE"),NSLOCTEXT("DroneOps","Status.ALERT_RESPONSE","ALERT_RESPONSE")},
{TEXT("Status.MISSION_EXECUTION"),NSLOCTEXT("DroneOps","Status.MISSION_EXECUTION","MISSION_EXECUTION")},
{TEXT("Status.ALL"),NSLOCTEXT("DroneOps","Status.ALL","ALL")},
{TEXT("Status.SYSTEM"),NSLOCTEXT("DroneOps","Status.SYSTEM","SYSTEM")},
{TEXT("Status.OPERATION"),NSLOCTEXT("DroneOps","Status.OPERATION","OPERATION")},
{TEXT("Status.ALERT"),NSLOCTEXT("DroneOps","Status.ALERT","ALERT")},
{TEXT("Status.MISSION"),NSLOCTEXT("DroneOps","Status.MISSION","MISSION")},
{TEXT("Status.PLAN"),NSLOCTEXT("DroneOps","Status.PLAN","PLAN")},
{TEXT("Events.PLAN_CREATED"),NSLOCTEXT("DroneOps","Events.PLAN_CREATED","Plan {plan_name} created")},
{TEXT("Events.PLAN_UPDATED"),NSLOCTEXT("DroneOps","Events.PLAN_UPDATED","Plan {plan_name} updated")},
{TEXT("Events.PLAN_COPIED"),NSLOCTEXT("DroneOps","Events.PLAN_COPIED","Plan {plan_name} copied as draft")},
{TEXT("Events.MISSION_CREATED"),NSLOCTEXT("DroneOps","Events.MISSION_CREATED","Mission {mission_name} created")},
{TEXT("Events.MISSION_UPDATED"),NSLOCTEXT("DroneOps","Events.MISSION_UPDATED","Mission {mission_name} renamed")},
{TEXT("Events.MISSION_DELETED"),NSLOCTEXT("DroneOps","Events.MISSION_DELETED","Mission {target_id} deleted")},
{TEXT("Events.UAV_ASSIGNED"),NSLOCTEXT("DroneOps","Events.UAV_ASSIGNED","{uav_id} assigned to mission {mission_name}")},
{TEXT("Events.UAV_UNASSIGNED"),NSLOCTEXT("DroneOps","Events.UAV_UNASSIGNED","Mission {mission_name} UAV unassigned")},
{TEXT("Events.ROUTE_CREATED"),NSLOCTEXT("DroneOps","Events.ROUTE_CREATED","Mission {mission_name} route saved")},
{TEXT("Events.ROUTE_UPDATED"),NSLOCTEXT("DroneOps","Events.ROUTE_UPDATED","Mission {mission_name} route updated")},
{TEXT("Events.ROUTE_CLEARED"),NSLOCTEXT("DroneOps","Events.ROUTE_CLEARED","Mission {mission_name} route cleared")},
{TEXT("Events.PLAN_VALIDATED"),NSLOCTEXT("DroneOps","Events.PLAN_VALIDATED","Plan {plan_name} configuration checked")},
{TEXT("Events.PLAN_REVIEWED"),NSLOCTEXT("DroneOps","Events.PLAN_REVIEWED","Plan {plan_name} reviewed")},
{TEXT("Events.PLAN_DEPLOYED"),NSLOCTEXT("DroneOps","Events.PLAN_DEPLOYED","Plan {plan_name} deployed. Execution has not started.")},
{TEXT("Common.Sync"),NSLOCTEXT("DroneOps","Common.Sync","BACKEND {0} | SYNC {1} | ACTIVE {2} | v{3}")},
{TEXT("Common.Connected"),NSLOCTEXT("DroneOps","Common.Connected","CONNECTED")},
{TEXT("Common.Disconnected"),NSLOCTEXT("DroneOps","Common.Disconnected","DISCONNECTED")},
{TEXT("Common.Reconnecting"),NSLOCTEXT("DroneOps","Common.Reconnecting","RECONNECTING")},
{TEXT("Tactical.Header"),NSLOCTEXT("DroneOps","Tactical.Header","2D TACTICAL MAP | Z{0} | {1} UAV without GPS")},
{TEXT("Tactical.Active"),NSLOCTEXT("DroneOps","Tactical.Active","ACTIVE")},
{TEXT("Tactical.Alert"),NSLOCTEXT("DroneOps","Tactical.Alert","ALERT")},
{TEXT("Fleet.Summary"),NSLOCTEXT("DroneOps","Fleet.Summary","Selected: {0} UAVs\n{1}")},
{TEXT("Fleet.Local"),NSLOCTEXT("DroneOps","Fleet.Local","Local rehearsal: backend refresh disabled")},
{TEXT("Fleet.Online"),NSLOCTEXT("DroneOps","Fleet.Online","Online")},
{TEXT("Fleet.Offline"),NSLOCTEXT("DroneOps","Fleet.Offline","Offline")},
{TEXT("Fleet.Lost"),NSLOCTEXT("DroneOps","Fleet.Lost","Disconnected")},
{TEXT("Fleet.Idle"),NSLOCTEXT("DroneOps","Fleet.Idle","Idle")},
{TEXT("Fleet.Unknown"),NSLOCTEXT("DroneOps","Fleet.Unknown","Unknown")},
{TEXT("Alarm.Severity"),NSLOCTEXT("DroneOps","Alarm.Severity","{0} / {1} UTC {2}")},
{TEXT("Alarm.Target"),NSLOCTEXT("DroneOps","Alarm.Target","TARGET UAV {0} | {1}")},
{TEXT("Alarm.Handled"),NSLOCTEXT("DroneOps","Alarm.Handled","Handled")},
{TEXT("Alarm.Info"),NSLOCTEXT("DroneOps","Alarm.Info","INFO")},
{TEXT("Alarm.Warning"),NSLOCTEXT("DroneOps","Alarm.Warning","WARNING")},
{TEXT("Alarm.Critical"),NSLOCTEXT("DroneOps","Alarm.Critical","CRITICAL")},
{TEXT("Log.All"),NSLOCTEXT("DroneOps","Log.All","ALL")},
{TEXT("Errors.COORDINATES_NOT_READY"),NSLOCTEXT("DroneOps","Errors.COORDINATES_NOT_READY","Map coordinates are not ready. Retry when the map is loaded.")},
{TEXT("Video.System"),NSLOCTEXT("DroneOps","Video.System","SYSTEM ONLINE: N/A | {0} UAVs | {1} VIEW POSITIONS | {2} PLAYING | AI: NOT CONNECTED | {3}")},
{TEXT("Plan.EditingBlock"),NSLOCTEXT("DroneOps","Plan.EditingBlock","Route editing in progress: {0}. Review and deployment are blocked.")},
{TEXT("Errors.ROLE_FORBIDDEN"),NSLOCTEXT("DroneOps","Errors.ROLE_FORBIDDEN","This client cannot perform this action.")},
{TEXT("Errors.REQUEST_REJECTED"),NSLOCTEXT("DroneOps","Errors.REQUEST_REJECTED","The server rejected this request. Refresh and retry.")},
{TEXT("Video.NoTargetHint"),NSLOCTEXT("DroneOps","Video.NoTargetHint","NO VIDEO TARGET\nChoose View Video to open a UAV feed.")},
{TEXT("Fleet.Primary"),NSLOCTEXT("DroneOps","Fleet.Primary","PRIMARY")},
{TEXT("Fleet.Multi"),NSLOCTEXT("DroneOps","Fleet.Multi","MULTI SELECTED")},
{TEXT("Video.DescriptorTip"),NSLOCTEXT("DroneOps","Video.DescriptorTip","{0}\nConfigured visual: {1}\nAircraft model is not supplied by Descriptor.")},
{TEXT("Events.backend_connected"),NSLOCTEXT("DroneOps","Events.backend_connected","Backend connected")},
{TEXT("Events.backend_reconnected"),NSLOCTEXT("DroneOps","Events.backend_reconnected","Backend reconnected")},
{TEXT("Events.backend_disconnected"),NSLOCTEXT("DroneOps","Events.backend_disconnected","Backend disconnected")},
{TEXT("Events.client_connected"),NSLOCTEXT("DroneOps","Events.client_connected","{source} client connected")},
{TEXT("Events.client_disconnected"),NSLOCTEXT("DroneOps","Events.client_disconnected","{source} client disconnected")},
{TEXT("Events.active_uav_changed"),NSLOCTEXT("DroneOps","Events.active_uav_changed","Active UAV changed: {target}")},
{TEXT("Events.alert_selected"),NSLOCTEXT("DroneOps","Events.alert_selected","Alert selected: {target}")},
{TEXT("Events.operation_mode_changed"),NSLOCTEXT("DroneOps","Events.operation_mode_changed","Operation mode changed: {target}")},
{TEXT("Events.enter_alert_response"),NSLOCTEXT("DroneOps","Events.enter_alert_response","Entered alert response")},
{TEXT("Events.active_mission_id_changed"),NSLOCTEXT("DroneOps","Events.active_mission_id_changed","Mission selected: {target}")},
{TEXT("Events.active_security_plan_id_changed"),NSLOCTEXT("DroneOps","Events.active_security_plan_id_changed","Plan selected: {target}")},
{TEXT("Events.active_area_id_changed"),NSLOCTEXT("DroneOps","Events.active_area_id_changed","Area selected: {target}")},
{TEXT("Stage1.Name"),NSLOCTEXT("DroneOps","Stage1.Name","DRONE SECURITY COMMAND SYSTEM")},
{TEXT("Stage1.Command"),NSLOCTEXT("DroneOps","Stage1.Command","COMMAND CENTER")},
{TEXT("Stage1.Map"),NSLOCTEXT("DroneOps","Stage1.Map","TACTICAL MAP")},
{TEXT("Stage1.Video"),NSLOCTEXT("DroneOps","Stage1.Video","VIDEO INTELLIGENCE")},
{TEXT("Stage1.Identity"),NSLOCTEXT("DroneOps","Stage1.Identity","{0}  |  {1}")},
{TEXT("Stage1.Health"),NSLOCTEXT("DroneOps","Stage1.Health","{0}   |   CMD {1}   MAP {2}   VID {3}")},
{TEXT("Stage1.System.READY"),NSLOCTEXT("DroneOps","Stage1.System.READY","SYSTEM READY")},
{TEXT("Stage1.System.DEGRADED"),NSLOCTEXT("DroneOps","Stage1.System.DEGRADED","SYSTEM DEGRADED")},
{TEXT("Stage1.System.OFFLINE"),NSLOCTEXT("DroneOps","Stage1.System.OFFLINE","BACKEND OFFLINE / RECONNECTING")},
{TEXT("Stage1.Online"),NSLOCTEXT("DroneOps","Stage1.Online","ONLINE")},
{TEXT("Stage1.Offline"),NSLOCTEXT("DroneOps","Stage1.Offline","OFFLINE")},
{TEXT("Stage1.Detail"),NSLOCTEXT("DroneOps","Stage1.Detail","Language {0}  |  {1}")},
{TEXT("Launcher.Layout"),NSLOCTEXT("DroneOps","Launcher.Layout","SINGLE-SCREEN MULTI-WINDOW  |  Focus a window for detailed work")},
{TEXT("Launcher.Start"),NSLOCTEXT("DroneOps","Launcher.Start","Launch System")},
{TEXT("Launcher.Shutdown"),NSLOCTEXT("DroneOps","Launcher.Shutdown","Shutdown System")},
{TEXT("Launcher.Restart"),NSLOCTEXT("DroneOps","Launcher.Restart","Restart")},
{TEXT("Launcher.Stop"),NSLOCTEXT("DroneOps","Launcher.Stop","Stop")},
{TEXT("Launcher.Focus"),NSLOCTEXT("DroneOps","Launcher.Focus","Focus")},
{TEXT("Launcher.Arrange"),NSLOCTEXT("DroneOps","Launcher.Arrange","Arrange Windows")},
{TEXT("Launcher.Existing"),NSLOCTEXT("DroneOps","Launcher.Existing","CONNECTED TO EXISTING INSTANCE")},
{TEXT("Launcher.Error"),NSLOCTEXT("DroneOps","Launcher.Error","Operation failed:")},
{TEXT("Launcher.STARTING"),NSLOCTEXT("DroneOps","Launcher.STARTING","STARTING")},
{TEXT("Launcher.READY"),NSLOCTEXT("DroneOps","Launcher.READY","SYSTEM READY")},
{TEXT("Launcher.DEGRADED"),NSLOCTEXT("DroneOps","Launcher.DEGRADED","SYSTEM DEGRADED")},
{TEXT("Launcher.FAILED"),NSLOCTEXT("DroneOps","Launcher.FAILED","FAILED")},
{TEXT("Launcher.STOPPING"),NSLOCTEXT("DroneOps","Launcher.STOPPING","STOPPING")},
{TEXT("Launcher.STOPPED"),NSLOCTEXT("DroneOps","Launcher.STOPPED","STOPPED")},
{TEXT("Launcher.ONLINE"),NSLOCTEXT("DroneOps","Launcher.ONLINE","ONLINE")},
{TEXT("Launcher.OFFLINE"),NSLOCTEXT("DroneOps","Launcher.OFFLINE","OFFLINE")},
{TEXT("Launcher.RECONNECTING"),NSLOCTEXT("DroneOps","Launcher.RECONNECTING","RECONNECTING")},
{TEXT("Nav.Overview"),NSLOCTEXT("DroneOps","Nav.Overview","Overview")},
{TEXT("Nav.Plans"),NSLOCTEXT("DroneOps","Nav.Plans","Security Plans")},
{TEXT("Nav.Fleet"),NSLOCTEXT("DroneOps","Nav.Fleet","UAV Fleet")},
{TEXT("Nav.Alerts"),NSLOCTEXT("DroneOps","Nav.Alerts","Alerts")},
{TEXT("Nav.Logs"),NSLOCTEXT("DroneOps","Nav.Logs","Logs")},
{TEXT("Workflow.NewPlan"),NSLOCTEXT("DroneOps","Workflow.NewPlan","+ New security plan")},
{TEXT("Workflow.BackPlans"),NSLOCTEXT("DroneOps","Workflow.BackPlans","Back to security plans")},
{TEXT("Workflow.OpenPlan"),NSLOCTEXT("DroneOps","Workflow.OpenPlan","Open plan")},
{TEXT("Workflow.CreateContinue"),NSLOCTEXT("DroneOps","Workflow.CreateContinue","Create and continue")},
{TEXT("Workflow.NextTask"),NSLOCTEXT("DroneOps","Workflow.NextTask","Next: task configuration")},
{TEXT("Workflow.BasicInfo"),NSLOCTEXT("DroneOps","Workflow.BasicInfo","Basic information")},
{TEXT("Workflow.TaskConfig"),NSLOCTEXT("DroneOps","Workflow.TaskConfig","Task configuration")},
{TEXT("Workflow.RouteEditing"),NSLOCTEXT("DroneOps","Workflow.RouteEditing","Route editing")},
{TEXT("Workflow.PreDeployReview"),NSLOCTEXT("DroneOps","Workflow.PreDeployReview","Pre-deployment check")},
{TEXT("Workflow.Deployed"),NSLOCTEXT("DroneOps","Workflow.Deployed","Deployed")},
{TEXT("Workflow.EditMap"),NSLOCTEXT("DroneOps","Workflow.EditMap","Edit Route in Map →")},
{TEXT("Workflow.ReturnMap"),NSLOCTEXT("DroneOps","Workflow.ReturnMap","Switch to Map / Retry")},
{TEXT("Workflow.BackEdit"),NSLOCTEXT("DroneOps","Workflow.BackEdit","Return to task configuration")},
{TEXT("Workflow.ConfirmDeploy"),NSLOCTEXT("DroneOps","Workflow.ConfirmDeploy","Confirm and deploy →")},
{TEXT("Workflow.NewVersion"),NSLOCTEXT("DroneOps","Workflow.NewVersion","Create a new version from this plan")},
{TEXT("Workflow.DefaultTask"),NSLOCTEXT("DroneOps","Workflow.DefaultTask","Task 01")},
{TEXT("Workflow.Step"),NSLOCTEXT("DroneOps","Workflow.Step","{0}  {1}")},
{TEXT("Workflow.ActiveStep"),NSLOCTEXT("DroneOps","Workflow.ActiveStep","● {0}  {1}")},
{TEXT("Workflow.PlanCard"),NSLOCTEXT("DroneOps","Workflow.PlanCard","{0}  ·  Tasks: {1}\nLast modified: {2}")},
{TEXT("Workflow.MapEditingStatus"),NSLOCTEXT("DroneOps","Workflow.MapEditingStatus","Editing in Map\n{0}\n{1} · {2}\nSaved waypoints: {3}")},
{TEXT("Workflow.MapOfflineStatus"),NSLOCTEXT("DroneOps","Workflow.MapOfflineStatus","Map client offline\nYour saved draft is retained.\n{0}\n{1} · {2}\nSaved waypoints: {3}")},
{TEXT("Workflow.SaveDraft"),NSLOCTEXT("DroneOps","Workflow.SaveDraft","Save draft")},
{TEXT("Workflow.FinishRoute"),NSLOCTEXT("DroneOps","Workflow.FinishRoute","Finish route editing")},
{TEXT("Workflow.ContinueEditing"),NSLOCTEXT("DroneOps","Workflow.ContinueEditing","Continue editing")},
{TEXT("Workflow.CurrentPlan"),NSLOCTEXT("DroneOps","Workflow.CurrentPlan","Current security plan\n{0}\n{1}")},
{TEXT("Workflow.RecentEvents"),NSLOCTEXT("DroneOps","Workflow.RecentEvents","Recent events")},
{TEXT("Workflow.GlobalHint"),NSLOCTEXT("DroneOps","Workflow.GlobalHint","Create → Configure → Edit route → Check → Deploy")},
{TEXT("Workflow.DeploymentNotice"),NSLOCTEXT("DroneOps","Workflow.DeploymentNotice","Configuration is deployed. The UAV has not started executing.")},
{TEXT("Events.ROUTE_EDIT_STARTED"),NSLOCTEXT("DroneOps","Events.ROUTE_EDIT_STARTED","Route editing started: {mission_name}")},
{TEXT("Events.ROUTE_SAVED"),NSLOCTEXT("DroneOps","Events.ROUTE_SAVED","Route draft saved: {mission_name}")},
{TEXT("Events.ROUTE_EDIT_COMPLETED"),NSLOCTEXT("DroneOps","Events.ROUTE_EDIT_COMPLETED","Route editing completed: {mission_name}")},
{TEXT("Workflow.Chinese"),NSLOCTEXT("DroneOps","Workflow.Chinese","中文")},
{TEXT("Workflow.English"),NSLOCTEXT("DroneOps","Workflow.English","English")},
{TEXT("Workflow.Undo"),NSLOCTEXT("DroneOps","Workflow.Undo","Undo")},
{TEXT("Workflow.EditSpeed"),NSLOCTEXT("DroneOps","Workflow.EditSpeed","Edit speed")},
{TEXT("Workflow.Waypoint"),NSLOCTEXT("DroneOps","Workflow.Waypoint","Waypoint {0}  ·  Height {1} m  ·  Speed {2} m/s")},
{TEXT("Workflow.RouteStats"),NSLOCTEXT("DroneOps","Workflow.RouteStats","Distance: {0} m · Estimated: {1} min (minimum estimate speed 1 m/s)")},
{TEXT("Events.PLAN_DELETED"),NSLOCTEXT("DroneOps","Events.PLAN_DELETED","Plan removed; history retained: {plan_name}")},
{TEXT("Workflow.DeleteDraft"),NSLOCTEXT("DroneOps","Workflow.DeleteDraft","Confirm delete draft")},
{TEXT("Workflow.Duplicate"),NSLOCTEXT("DroneOps","Workflow.Duplicate","Duplicate")},
{TEXT("Workflow.SaveExit"),NSLOCTEXT("DroneOps","Workflow.SaveExit","Save and exit")},
{TEXT("Workflow.AssignedUAV"),NSLOCTEXT("DroneOps","Workflow.AssignedUAV","Assigned UAV")},
{TEXT("Workflow.Patrol"),NSLOCTEXT("DroneOps","Workflow.Patrol","Task type: Patrol")},
{TEXT("Workflow.StepInfo"),NSLOCTEXT("DroneOps","Workflow.StepInfo","Info")},
{TEXT("Workflow.StepTask"),NSLOCTEXT("DroneOps","Workflow.StepTask","Tasks")},
{TEXT("Workflow.StepRoute"),NSLOCTEXT("DroneOps","Workflow.StepRoute","Route")},
{TEXT("Workflow.StepCheck"),NSLOCTEXT("DroneOps","Workflow.StepCheck","Check")},
{TEXT("Workflow.StepDeploy"),NSLOCTEXT("DroneOps","Workflow.StepDeploy","Deploy")},
{TEXT("Workflow.DeploymentSuccess"),NSLOCTEXT("DroneOps","Workflow.DeploymentSuccess","✓ Security plan deployed")},
{TEXT("Workflow.CheckInformation"),NSLOCTEXT("DroneOps","Workflow.CheckInformation","Check task assignment and saved route below.")},
{TEXT("Workflow.Readiness"),NSLOCTEXT("DroneOps","Workflow.Readiness","Backend: {0}   ·   Map: {1}")},
{TEXT("Workflow.RouteEditBanner"),NSLOCTEXT("DroneOps","Workflow.RouteEditBanner","● ROUTE EDITING")},
{TEXT("Map.SAVED"),NSLOCTEXT("DroneOps","Map.SAVED","Saved · editing continues")},
{TEXT("Workflow.VersionName"),NSLOCTEXT("DroneOps","Workflow.VersionName","{0} — new version")},
{TEXT("Workflow.LastSaved"),NSLOCTEXT("DroneOps","Workflow.LastSaved","Last saved: {0}")},
{TEXT("Events.PLAN_OPENED"),NSLOCTEXT("DroneOps","Events.PLAN_OPENED","Opened {plan_name}")},
{TEXT("Events.PLAN_VERSION_CREATED"),NSLOCTEXT("DroneOps","Events.PLAN_VERSION_CREATED","Created a new version: {plan_name}")},
{TEXT("Events.PLAN_REVIEW_READY"),NSLOCTEXT("DroneOps","Events.PLAN_REVIEW_READY","Pre-deployment check ready: {plan_name}")},
{TEXT("Events.MISSION_CONFIGURED"),NSLOCTEXT("DroneOps","Events.MISSION_CONFIGURED","Task configured: {mission_name} · {uav_id}")},
{TEXT("Errors.MAP_FOCUS_MANUAL"),NSLOCTEXT("DroneOps","Errors.MAP_FOCUS_MANUAL","Select the Map window or use Launcher Focus.")},
{TEXT("Workflow.CreatedAt"),NSLOCTEXT("DroneOps","Workflow.CreatedAt","Created: {0}")},
{TEXT("Workflow.ReviewDistance"),NSLOCTEXT("DroneOps","Workflow.ReviewDistance","Route distance: {0} m")},
{TEXT("Execution.Deploy"),NSLOCTEXT("DroneOps","Execution.Deploy","Deploy Plan →")},
{TEXT("Execution.Start"),NSLOCTEXT("DroneOps","Execution.Start","Start Mission →")},
{TEXT("Execution.Open"),NSLOCTEXT("DroneOps","Execution.Open","Open mission")},
{TEXT("Execution.None"),NSLOCTEXT("DroneOps","Execution.None","No active mission")},
{TEXT("Execution.Simulation"),NSLOCTEXT("DroneOps","Execution.Simulation","SIMULATION · Mock UAV")},
{TEXT("Execution.Monitor"),NSLOCTEXT("DroneOps","Execution.Monitor","Execution Monitor · SIMULATION")},
{TEXT("Execution.ReadOnly"),NSLOCTEXT("DroneOps","Execution.ReadOnly","Deployed route is read-only")},
{TEXT("Execution.Summary"),NSLOCTEXT("DroneOps","Execution.Summary","{0}\n{1} · {2}\n{3}\nWaypoint {4} / {5} · {6}")},
{TEXT("Execution.Confirm"),NSLOCTEXT("DroneOps","Execution.Confirm","Confirm")},
{TEXT("Execution.Pause"),NSLOCTEXT("DroneOps","Execution.Pause","Pause mission")},
{TEXT("Execution.Resume"),NSLOCTEXT("DroneOps","Execution.Resume","Resume mission")},
{TEXT("Execution.Return"),NSLOCTEXT("DroneOps","Execution.Return","Return Home")},
{TEXT("Execution.Abort"),NSLOCTEXT("DroneOps","Execution.Abort","Abort mission")},
{TEXT("Execution.Cancel"),NSLOCTEXT("DroneOps","Execution.Cancel","Cancel")},
{TEXT("Execution.History"),NSLOCTEXT("DroneOps","Execution.History","Execution history / View plan")},
{TEXT("Execution.Video"),NSLOCTEXT("DroneOps","Execution.Video","View mission UAV video")},
{TEXT("Execution.Duration"),NSLOCTEXT("DroneOps","Execution.Duration","Execution time: {0} seconds")},
{TEXT("Execution.StartConfirm"),NSLOCTEXT("DroneOps","Execution.StartConfirm","Confirm mission execution")},
{TEXT("Execution.MockCheck"),NSLOCTEXT("DroneOps","Execution.MockCheck","Simulation preflight checks deployment, route, Mock UAV availability and execution conflicts. Backend confirms every action.")},
{TEXT("Execution.returnConfirm"),NSLOCTEXT("DroneOps","Execution.returnConfirm","Confirm Return Home? Remaining waypoints will be skipped. The Mock UAV will return to its defined Home.")},
{TEXT("Execution.abortConfirm"),NSLOCTEXT("DroneOps","Execution.abortConfirm","Confirm abort? This execution cannot be resumed. The Mock UAV will stop at its current position.")},
{TEXT("Execution.ShutdownConfirm"),NSLOCTEXT("DroneOps","Execution.ShutdownConfirm","A simulated mission is active. Shutdown will persist it as paused and stop local simulation. Continue?")},
{TEXT("Execution.ROUTE_FINISHED"),NSLOCTEXT("DroneOps","Execution.ROUTE_FINISHED","All waypoints completed")},
{TEXT("Execution.RETURNED_HOME"),NSLOCTEXT("DroneOps","Execution.RETURNED_HOME","Returned to simulation Home")},
{TEXT("Execution.CREATED"),NSLOCTEXT("DroneOps","Execution.CREATED","Mission created")},
{TEXT("Execution.PREFLIGHT"),NSLOCTEXT("DroneOps","Execution.PREFLIGHT","Checking simulation conditions")},
{TEXT("Execution.STARTING"),NSLOCTEXT("DroneOps","Execution.STARTING","Starting mission…")},
{TEXT("Execution.EXECUTING"),NSLOCTEXT("DroneOps","Execution.EXECUTING","Mission executing")},
{TEXT("Execution.PAUSED"),NSLOCTEXT("DroneOps","Execution.PAUSED","Mission paused")},
{TEXT("Execution.RETURNING"),NSLOCTEXT("DroneOps","Execution.RETURNING","Returning Home")},
{TEXT("Execution.COMPLETED"),NSLOCTEXT("DroneOps","Execution.COMPLETED","Mission completed")},
{TEXT("Execution.ABORTED"),NSLOCTEXT("DroneOps","Execution.ABORTED","Mission aborted")},
{TEXT("Execution.FAILED"),NSLOCTEXT("DroneOps","Execution.FAILED","Mission failed")},
{TEXT("Errors.MOCK_UAV_UNAVAILABLE"),NSLOCTEXT("DroneOps","Errors.MOCK_UAV_UNAVAILABLE","Mock UAV unavailable. Check the simulation fixture and registry.")},
{TEXT("Errors.EXECUTION_CONFLICT"),NSLOCTEXT("DroneOps","Errors.EXECUTION_CONFLICT","This UAV already has an active mission.")},
{TEXT("Errors.EXECUTION_STALE"),NSLOCTEXT("DroneOps","Errors.EXECUTION_STALE","Mission state changed. Review the current state and retry.")},
{TEXT("Errors.EXECUTION_STATE_INVALID"),NSLOCTEXT("DroneOps","Errors.EXECUTION_STATE_INVALID","This action is unavailable in the current mission state.")},
{TEXT("Errors.EXECUTION_NOT_FOUND"),NSLOCTEXT("DroneOps","Errors.EXECUTION_NOT_FOUND","Execution no longer exists.")},
{TEXT("Errors.IMMUTABLE_ROUTE_MISSING"),NSLOCTEXT("DroneOps","Errors.IMMUTABLE_ROUTE_MISSING","The deployed route snapshot is missing.")},
{TEXT("Errors.REQUEST_ID_REQUIRED"),NSLOCTEXT("DroneOps","Errors.REQUEST_ID_REQUIRED","Execution request identifier is required.")},
{TEXT("Errors.REQUEST_ID_REUSED"),NSLOCTEXT("DroneOps","Errors.REQUEST_ID_REUSED","Request identifier conflict.")},
{TEXT("Errors.DEPLOYMENT_MISSING"),NSLOCTEXT("DroneOps","Errors.DEPLOYMENT_MISSING","Deployment snapshot is missing.")},
{TEXT("Events.EXECUTION_CREATED"),NSLOCTEXT("DroneOps","Events.EXECUTION_CREATED","Mission execution created · {uav_id}")},
{TEXT("Events.PREFLIGHT_STARTED"),NSLOCTEXT("DroneOps","Events.PREFLIGHT_STARTED","Simulation preflight started · {uav_id}")},
{TEXT("Events.PREFLIGHT_PASSED"),NSLOCTEXT("DroneOps","Events.PREFLIGHT_PASSED","Simulation preflight passed · {uav_id}")},
{TEXT("Events.MISSION_STARTING"),NSLOCTEXT("DroneOps","Events.MISSION_STARTING","Mission starting · {uav_id}")},
{TEXT("Events.MISSION_STARTED"),NSLOCTEXT("DroneOps","Events.MISSION_STARTED","Mission started · {uav_id}")},
{TEXT("Events.MISSION_PAUSED"),NSLOCTEXT("DroneOps","Events.MISSION_PAUSED","Mission paused · {uav_id}")},
{TEXT("Events.MISSION_RESUMED"),NSLOCTEXT("DroneOps","Events.MISSION_RESUMED","Mission resumed · {uav_id}")},
{TEXT("Events.WAYPOINT_REACHED"),NSLOCTEXT("DroneOps","Events.WAYPOINT_REACHED","Waypoint reached · {uav_id}")},
{TEXT("Events.MISSION_RETURNING"),NSLOCTEXT("DroneOps","Events.MISSION_RETURNING","Returning Home · {uav_id}")},
{TEXT("Events.MISSION_ABORTED"),NSLOCTEXT("DroneOps","Events.MISSION_ABORTED","Mission aborted · {uav_id}")},
{TEXT("Events.MOCK_STARTUP_RESET"),NSLOCTEXT("DroneOps","Events.MOCK_STARTUP_RESET","Mock aircraft returned to the startup formation")},
{TEXT("Errors.MOCK_STARTUP_RESET"),NSLOCTEXT("DroneOps","Errors.MOCK_STARTUP_RESET","Ended when the Mock session restarted and aircraft returned to formation.")},
{TEXT("Events.MISSION_COMPLETED"),NSLOCTEXT("DroneOps","Events.MISSION_COMPLETED","Mission completed · {uav_id}")},
{TEXT("Events.MISSION_FAILED"),NSLOCTEXT("DroneOps","Events.MISSION_FAILED","Mission failed · {uav_id}")},
{TEXT("Execution.Estimate"),NSLOCTEXT("DroneOps","Execution.Estimate","Estimated distance: {0} m · Time: {1} seconds")},
{TEXT("Execution.MockAvailable"),NSLOCTEXT("DroneOps","Execution.MockAvailable","✓ Mock UAV configured")},
{TEXT("Events.DEPLOYMENT_CREATED"),NSLOCTEXT("DroneOps","Events.DEPLOYMENT_CREATED","Deployment created: {plan_name}")},
{TEXT("Execution.ConfirmStart"),NSLOCTEXT("DroneOps","Execution.ConfirmStart","Confirm Start Mission")},
{TEXT("Execution.ConfirmReturn"),NSLOCTEXT("DroneOps","Execution.ConfirmReturn","Confirm Return")},
{TEXT("Execution.ConfirmAbort"),NSLOCTEXT("DroneOps","Execution.ConfirmAbort","Confirm Abort")},
{TEXT("Execution.NoConflict"),NSLOCTEXT("DroneOps","Execution.NoConflict","No active execution conflict")},
{TEXT("Execution.DeploymentValid"),NSLOCTEXT("DroneOps","Execution.DeploymentValid","Immutable deployment and saved route")},
{TEXT("Geometry.ClosedRoute"),NSLOCTEXT("DroneOps","Geometry.ClosedRoute","Closed Route")},
{TEXT("Geometry.MovePlan"),NSLOCTEXT("DroneOps","Geometry.MovePlan","Move Plan")},
{TEXT("Geometry.Moving"),NSLOCTEXT("DroneOps","Geometry.Moving","Moving Security Plan: {0}")},
{TEXT("Geometry.Drag"),NSLOCTEXT("DroneOps","Geometry.Drag","Drag the center handle to move all routes.")},
{TEXT("Geometry.Preview"),NSLOCTEXT("DroneOps","Geometry.Preview","Preview only — confirm position to save.")},
{TEXT("Geometry.Confirm"),NSLOCTEXT("DroneOps","Geometry.Confirm","Confirm Position")},
{TEXT("Errors.CLOSED_ROUTE_TOO_SHORT"),NSLOCTEXT("DroneOps","Errors.CLOSED_ROUTE_TOO_SHORT","Closed route requires at least 3 waypoints.")},
{TEXT("Errors.PLAN_EXECUTING"),NSLOCTEXT("DroneOps","Errors.PLAN_EXECUTING","This plan has active or scheduled tasks. Finish or cancel them before editing, moving or deleting the plan.")},
{TEXT("Errors.INVALID_TRANSLATION"),NSLOCTEXT("DroneOps","Errors.INVALID_TRANSLATION","The route changed. Cancel and retry moving the plan.")},
{TEXT("Events.PLAN_TRANSLATED"),NSLOCTEXT("DroneOps","Events.PLAN_TRANSLATED","Plan position confirmed")},
{TEXT("Events.ROUTE_CLOSURE_REACHED"),NSLOCTEXT("DroneOps","Events.ROUTE_CLOSURE_REACHED","Closed route completed once")},
{TEXT("Mission3D.Altitude"),NSLOCTEXT("DroneOps","Mission3D.Altitude","Altitude (m)")},
{TEXT("Mission3D.Hover"),NSLOCTEXT("DroneOps","Mission3D.Hover","Hover time (seconds)")},
{TEXT("Mission3D.AGL"),NSLOCTEXT("DroneOps","Mission3D.AGL","AGL: above ground (unchecked: ellipsoid)")},
{TEXT("Mission3D.Apply"),NSLOCTEXT("DroneOps","Mission3D.Apply","APPLY WAYPOINT PARAMETERS")},
{TEXT("Mission3D.Selected"),NSLOCTEXT("DroneOps","Mission3D.Selected","Selected Waypoint: P{0}")},
{TEXT("Mission3D.SelectPoint"),NSLOCTEXT("DroneOps","Mission3D.SelectPoint","Select a waypoint to edit height and hover time")},
{TEXT("Formation.LineSpacing"),NSLOCTEXT("DroneOps","Formation.LineSpacing","Line formation spacing (m)")},
{TEXT("Formation.SaveAssignment"),NSLOCTEXT("DroneOps","Formation.SaveAssignment","SAVE ASSIGNED UAVS")},
{TEXT("Schedule.Enable"),NSLOCTEXT("DroneOps","Schedule.Enable","Scheduled start (unchecked: start now)")},
{TEXT("Schedule.LocalTime"),NSLOCTEXT("DroneOps","Schedule.LocalTime","Local start date/time: YYYY-MM-DDTHH:MM:SS")},
{TEXT("Schedule.StartNow"),NSLOCTEXT("DroneOps","Schedule.StartNow","START NOW")},
{TEXT("Schedule.Reschedule"),NSLOCTEXT("DroneOps","Schedule.Reschedule","RESCHEDULE")},
{TEXT("Schedule.Cancel"),NSLOCTEXT("DroneOps","Schedule.Cancel","CANCEL SCHEDULE")},
{TEXT("Schedule.Countdown"),NSLOCTEXT("DroneOps","Schedule.Countdown","Scheduled mission starts in {0}")},
{TEXT("Execution.SCHEDULED"),NSLOCTEXT("DroneOps","Execution.SCHEDULED","SCHEDULED")},
{TEXT("Execution.CANCELLED"),NSLOCTEXT("DroneOps","Execution.CANCELLED","CANCELLED")},
{TEXT("Situation.Title"),NSLOCTEXT("DroneOps","Situation.Title","2D SITUATION MAP")},
{TEXT("Situation.Follow"),NSLOCTEXT("DroneOps","Situation.Follow","TOGGLE FOLLOW SELECTED UAV")},
{TEXT("Errors.ALTITUDE_DATUM_UNRESOLVED"),NSLOCTEXT("DroneOps","Errors.ALTITUDE_DATUM_UNRESOLVED","Cannot resolve height. Select a waypoint, enter valid values and ensure terrain is loaded.")},
{TEXT("Errors.INVALID_ALTITUDE_REFERENCE"),NSLOCTEXT("DroneOps","Errors.INVALID_ALTITUDE_REFERENCE","Unknown altitude reference")},
{TEXT("Errors.INVALID_SEPARATION"),NSLOCTEXT("DroneOps","Errors.INVALID_SEPARATION","Spacing must meet the configured minimum separation.")},
{TEXT("Errors.TRAJECTORY_CONFLICT"),NSLOCTEXT("DroneOps","Errors.TRAJECTORY_CONFLICT","Trajectory conflict: aircraft cannot maintain the required separation.")},
{TEXT("Errors.INVALID_SCHEDULE"),NSLOCTEXT("DroneOps","Errors.INVALID_SCHEDULE","Enter a valid future local date and time.")},
{TEXT("Errors.FORMATION_UNSUPPORTED"),NSLOCTEXT("DroneOps","Errors.FORMATION_UNSUPPORTED","Only Line formation is currently available.")},
{TEXT("Errors.TRAJECTORY_DOMAIN_UNSUPPORTED"),NSLOCTEXT("DroneOps","Errors.TRAJECTORY_DOMAIN_UNSUPPORTED","Formation requires a nonpolar area within 10 km.")},
{TEXT("Mission3D.Marker"),NSLOCTEXT("DroneOps","Mission3D.Marker","P{0} | {1} m {2} | Hover {3} s")},
{TEXT("Group.Title"),NSLOCTEXT("DroneOps","Group.Title","GROUP CONTROL")},
{TEXT("Group.Summary"),NSLOCTEXT("DroneOps","Group.Summary","Selected UAVs: {0} | Preserve relative offsets | Minimum separation: {1} m")},
{TEXT("Group.Latitude"),NSLOCTEXT("DroneOps","Group.Latitude","Group anchor latitude (WGS84)")},
{TEXT("Group.Longitude"),NSLOCTEXT("DroneOps","Group.Longitude","Group anchor longitude (WGS84)")},
{TEXT("Group.Altitude"),NSLOCTEXT("DroneOps","Group.Altitude","Group anchor ellipsoid altitude (m)")},
{TEXT("Group.Area"),NSLOCTEXT("DroneOps","Group.Area","Available target area radius (m)")},
{TEXT("Group.Move"),NSLOCTEXT("DroneOps","Group.Move","MOVE GROUP")},
{TEXT("Group.Accepted"),NSLOCTEXT("DroneOps","Group.Accepted","Group command accepted by Backend")},
{TEXT("Group.Conflict"),NSLOCTEXT("DroneOps","Group.Conflict","{0} / {1}: predicted {2} m; required {3} m. Execution blocked.")},
{TEXT("Schedule.Overview"),NSLOCTEXT("DroneOps","Schedule.Overview","Scheduled: {0} | Starts in {1} | {2} UAV")},
{TEXT("Errors.TERRAIN_REQUIRED"),NSLOCTEXT("DroneOps","Errors.TERRAIN_REQUIRED","Terrain height is unavailable or the waypoint is below ground. Load terrain and retry.")},
{TEXT("Events.SCHEDULE_STARTED"),NSLOCTEXT("DroneOps","Events.SCHEDULE_STARTED","{uav_id}: scheduled mission started")},
{TEXT("Events.SCHEDULE_CHANGED"),NSLOCTEXT("DroneOps","Events.SCHEDULE_CHANGED","{uav_id}: start time updated")},
{TEXT("Events.SCHEDULE_DUE"),NSLOCTEXT("DroneOps","Events.SCHEDULE_DUE","{uav_id}: immediate start requested")},
{TEXT("Events.MISSION_CANCELLED"),NSLOCTEXT("DroneOps","Events.MISSION_CANCELLED","{uav_id}: scheduled mission cancelled")},
{TEXT("Events.SAFETY_CONFLICT"),NSLOCTEXT("DroneOps","Events.SAFETY_CONFLICT","{uav_id}: motion paused by safety separation check")},
{TEXT("Execution.Real"),NSLOCTEXT("DroneOps","Execution.Real","LIVE CONTROL · Real UAV")},
{TEXT("Execution.MonitorNeutral"),NSLOCTEXT("DroneOps","Execution.MonitorNeutral","Execution Monitor")},
{TEXT("Execution.RealAvailable"),NSLOCTEXT("DroneOps","Execution.RealAvailable","Real adapter configured; Backend checks live readiness")},
{TEXT("Execution.RealCheck"),NSLOCTEXT("DroneOps","Execution.RealCheck","Real control requires fresh measured telemetry, calibrated coordinates and Armed/Offboard before movement. Start does not arm or take off.")},
{TEXT("Execution.Real.returnConfirm"),NSLOCTEXT("DroneOps","Execution.Real.returnConfirm","Confirm Return Home? Remaining waypoints will be skipped and the real UAV will be commanded to its calibrated Home.")},
{TEXT("Execution.Real.abortConfirm"),NSLOCTEXT("DroneOps","Execution.Real.abortConfirm","Confirm abort? The mission cannot resume. Backend requests a hold at the measured position and reports any rejection.")},
{TEXT("Errors.REAL_COMMAND_REJECTED"),NSLOCTEXT("DroneOps","Errors.REAL_COMMAND_REJECTED","Real UAV rejected the command. Verify Armed/Offboard and telemetry.")},
{TEXT("Errors.REAL_HOLD_REJECTED"),NSLOCTEXT("DroneOps","Errors.REAL_HOLD_REJECTED","Hold was not confirmed. Check the aircraft and control link immediately.")},
{TEXT("Errors.REAL_TELEMETRY_UNAVAILABLE"),NSLOCTEXT("DroneOps","Errors.REAL_TELEMETRY_UNAVAILABLE","Fresh valid aircraft telemetry is unavailable.")},
{TEXT("Errors.REAL_ANCHOR_CHANGED_RECALIBRATE"),NSLOCTEXT("DroneOps","Errors.REAL_ANCHOR_CHANGED_RECALIBRATE","The live coordinate anchor changed. Recalibrate before control.")},
{TEXT("Errors.REAL_RESTART_REQUIRES_RESUME"),NSLOCTEXT("DroneOps","Errors.REAL_RESTART_REQUIRES_RESUME","Real mission restored paused; verify the aircraft before resuming.")},
{TEXT("Errors.ADAPTER_MODE_MISMATCH"),NSLOCTEXT("DroneOps","Errors.ADAPTER_MODE_MISMATCH","Stored execution belongs to a different adapter. Create a new execution.")},
{TEXT("Errors.REAL_HOVER_DRIFT"),NSLOCTEXT("DroneOps","Errors.REAL_HOVER_DRIFT","Measured UAV position left the hover tolerance.")},
{TEXT("Errors.GROUP_MEMBER_FAILED"),NSLOCTEXT("DroneOps","Errors.GROUP_MEMBER_FAILED","A group member failed; the entire mission group has failed and active members were asked to hold.")},
{TEXT("Errors.LEGACY_CONTROLLER_BUSY"),NSLOCTEXT("DroneOps","Errors.LEGACY_CONTROLLER_BUSY","Another controller owns the aircraft.")},
{TEXT("Errors.MISSION_CONTROLLER_OWNS_CONTROL"),NSLOCTEXT("DroneOps","Errors.MISSION_CONTROLLER_OWNS_CONTROL","Use the security mission controls in Real mode.")},
{TEXT("Execution.RealEstimate"),NSLOCTEXT("DroneOps","Execution.RealEstimate","Real execution estimates are calculated from live position by Backend at start.")},
{TEXT("Errors.REAL_UAV_UNAVAILABLE"),NSLOCTEXT("DroneOps","Errors.REAL_UAV_UNAVAILABLE","Real UAV or its calibrated frame is unavailable.")},
{TEXT("Errors.REAL_CONFIGURATION_INVALID"),NSLOCTEXT("DroneOps","Errors.REAL_CONFIGURATION_INVALID","Real adapter configuration is incomplete or invalid.")},
{TEXT("Errors.ADAPTER_POSITION_INVALID"),NSLOCTEXT("DroneOps","Errors.ADAPTER_POSITION_INVALID","Adapter position is not a valid WGS84 coordinate.")},
{TEXT("Errors.ADAPTER_COMMAND_INVALID"),NSLOCTEXT("DroneOps","Errors.ADAPTER_COMMAND_INVALID","The motion target or speed is invalid.")},
{TEXT("Errors.REAL_FRAME_DOMAIN_UNSUPPORTED"),NSLOCTEXT("DroneOps","Errors.REAL_FRAME_DOMAIN_UNSUPPORTED","The real mission is outside the supported local coordinate area.")},
{TEXT("Errors.REAL_NED_INVALID"),NSLOCTEXT("DroneOps","Errors.REAL_NED_INVALID","Measured NED position is invalid.")},
{TEXT("Errors.REAL_ANCHOR_DATUM_UNRESOLVED"),NSLOCTEXT("DroneOps","Errors.REAL_ANCHOR_DATUM_UNRESOLVED","Declare the real anchor altitude datum before execution.")},
{TEXT("Errors.REAL_ANCHOR_INVALID"),NSLOCTEXT("DroneOps","Errors.REAL_ANCHOR_INVALID","The real coordinate calibration is invalid.")},
{TEXT("Inspector.Title"),NSLOCTEXT("DroneOps","Inspector.Title","Waypoint Inspector")},
{TEXT("Inspector.Help"),NSLOCTEXT("DroneOps","Inspector.Help","Click a waypoint to select it. Click the map to add one. Enter or leave a field to update.")},
{TEXT("Inspector.Reference"),NSLOCTEXT("DroneOps","Inspector.Reference","Altitude reference")},
{TEXT("Inspector.Speed"),NSLOCTEXT("DroneOps","Inspector.Speed","Speed (m/s; 0 = default)")},
{TEXT("Inspector.Coordinates"),NSLOCTEXT("DroneOps","Inspector.Coordinates","Longitude: {0}\nLatitude: {1}")},
{TEXT("Errors.WAYPOINT_PARAMETERS_INVALID"),NSLOCTEXT("DroneOps","Errors.WAYPOINT_PARAMETERS_INVALID","Enter finite values: hover >= 0; speed 0 to 15 m/s.")},
{TEXT("Map.ViewLocked"),NSLOCTEXT("DroneOps","Map.ViewLocked","ROUTE EDITING | MAP VIEW LOCKED")},
{TEXT("Map.AdjustBeforeEdit"),NSLOCTEXT("DroneOps","Map.AdjustBeforeEdit","Adjust the map view before editing.")},
{TEXT("Map.FinishBeforeMode"),NSLOCTEXT("DroneOps","Map.FinishBeforeMode","Finish route editing before switching map mode.")},
{TEXT("Map.InvalidPosition"),NSLOCTEXT("DroneOps","Map.InvalidPosition","No valid map position")},
{TEXT("RouteVisual.Legend"),NSLOCTEXT("DroneOps","RouteVisual.Legend","ROUTE VISUAL  |  Low → High\nAltitude {0}–{1} m ({4})\nSpeed {2}–{3} m/s · Slow ▶  Fast ▶▶▶\nDirection →  |  Conflict: red pulse")},
{TEXT("RouteVisual.Ellipsoid"),NSLOCTEXT("DroneOps","RouteVisual.Ellipsoid","Ellipsoid")},
{TEXT("RouteVisual.WorldHeight"),NSLOCTEXT("DroneOps","RouteVisual.WorldHeight","World height")},
{TEXT("Reservation.InUse"),NSLOCTEXT("DroneOps","Reservation.InUse","{0} · In Use · {1}")},
{TEXT("Reservation.Conflict"),NSLOCTEXT("DroneOps","Reservation.Conflict","{0} is currently executing Security Plan {1} and cannot be assigned to another plan.")},
{TEXT("Errors.DRONE_ACTIVE_PLAN_CONFLICT"),NSLOCTEXT("DroneOps","Errors.DRONE_ACTIVE_PLAN_CONFLICT","This UAV is executing another security plan. Refresh the assignment and choose an available UAV.")},
{TEXT("Reservation.ClearUnavailable"),NSLOCTEXT("DroneOps","Reservation.ClearUnavailable","Clear unavailable selections (save to apply)")},
{TEXT("Fleet.BoxButton"),NSLOCTEXT("DroneOps","Fleet.BoxButton","Box select UAVs")},
{TEXT("Fleet.MoveButton"),NSLOCTEXT("DroneOps","Fleet.MoveButton","Fleet move")},
{TEXT("Fleet.Confirm"),NSLOCTEXT("DroneOps","Fleet.Confirm","Confirm fleet move")},
{TEXT("Fleet.Browse"),NSLOCTEXT("DroneOps","Fleet.Browse","Return to browse / Esc")},
{TEXT("Fleet.BoxHint"),NSLOCTEXT("DroneOps","Fleet.BoxHint","Drag a box. Shift adds, Ctrl toggles. Click empty space to clear.")},
{TEXT("Fleet.MoveHint"),NSLOCTEXT("DroneOps","Fleet.MoveHint","Click a target, review the routes, then confirm. The primary UAV is the formation anchor.")},
{TEXT("Fleet.BrowseHint"),NSLOCTEXT("DroneOps","Fleet.BrowseHint","Browse the map. Shift + drag also adds UAVs to the selection.")},
{TEXT("Fleet.Target"),NSLOCTEXT("DroneOps","Fleet.Target","Target: {0}, {1}\nAnchor ellipsoid height: {2} m. Relative positions and heights are preserved.")},
{TEXT("Fleet.Execution"),NSLOCTEXT("DroneOps","Fleet.Execution","Fleet execution: {0}")},
{TEXT("Fleet.SelectTwo"),NSLOCTEXT("DroneOps","Fleet.SelectTwo","Box select at least two UAVs first.")},
{TEXT("Fleet.SelectionPending"),NSLOCTEXT("DroneOps","Fleet.SelectionPending","Selection submitted; waiting for synchronization.")},
{TEXT("Fleet.SelectionChanged"),NSLOCTEXT("DroneOps","Fleet.SelectionChanged","Selection changed. Click a new target to review before moving.")},
{TEXT("Fleet.PreviewOnly"),NSLOCTEXT("DroneOps","Fleet.PreviewOnly","Preview only. Confirm to request Backend execution; Esc cancels.")},
{TEXT("Fleet.Occupied"),NSLOCTEXT("DroneOps","Fleet.Occupied","{0} is in use by {1}. Finish or abort that task before moving.")},
{TEXT("Fleet.TelemetryRequired"),NSLOCTEXT("DroneOps","Fleet.TelemetryRequired","{0} has no available position. Refresh the connection and retry.")},
{TEXT("Group.Owner"),NSLOCTEXT("DroneOps","Group.Owner","Group Move")},
{TEXT("Fleet.SelectionReady"),NSLOCTEXT("DroneOps","Fleet.SelectionReady","Selection synchronized.")},
{TEXT("Fleet3D.Height"),NSLOCTEXT("DroneOps","Fleet3D.Height","Anchor target height (ellipsoid m)")},
{TEXT("Fleet3D.Up"),NSLOCTEXT("DroneOps","Fleet3D.Up","+10 m")},
{TEXT("Fleet3D.Down"),NSLOCTEXT("DroneOps","Fleet3D.Down","-10 m")},
{TEXT("Fleet3D.Current"),NSLOCTEXT("DroneOps","Fleet3D.Current","Current")},
{TEXT("Fleet3D.HeightHint"),NSLOCTEXT("DroneOps","Fleet3D.HeightHint","Pick horizontal position on the current flight plane, then set height. Relative formation offsets are preserved.")},
{TEXT("Fleet3D.MoveHint"),NSLOCTEXT("DroneOps","Fleet3D.MoveHint","3D fleet move: choose a position and height, review the preview, then confirm.")},
{TEXT("Fleet3D.InvalidHeight"),NSLOCTEXT("DroneOps","Fleet3D.InvalidHeight","Enter a numeric height within 10,000 m of the current anchor height. No movement has been sent.")},
{TEXT("Fleet3D.ViewChanged"),NSLOCTEXT("DroneOps","Fleet3D.ViewChanged","View changed. Choose a new target before confirming movement.")},
{TEXT("Fleet3D.Marker"),NSLOCTEXT("DroneOps","Fleet3D.Marker","Target anchor: {0} m")},
{TEXT("FleetUX.Title"),NSLOCTEXT("DroneOps","FleetUX.Title","Fleet control")},
{TEXT("FleetUX.Step1"),NSLOCTEXT("DroneOps","FleetUX.Step1","1  Select")},
{TEXT("FleetUX.Step2"),NSLOCTEXT("DroneOps","FleetUX.Step2","2  Target")},
{TEXT("FleetUX.Step3"),NSLOCTEXT("DroneOps","FleetUX.Step3","3  Review")},
{TEXT("FleetUX.Height"),NSLOCTEXT("DroneOps","FleetUX.Height","Target height · m")},
{TEXT("FleetUX.Next"),NSLOCTEXT("DroneOps","FleetUX.Next","Next · choose destination")},
{TEXT("FleetUX.Again"),NSLOCTEXT("DroneOps","FleetUX.Again","Move again")},
{TEXT("FleetUX.Confirm"),NSLOCTEXT("DroneOps","FleetUX.Confirm","Move {0} aircraft")},
{TEXT("FleetUX.NeedTarget"),NSLOCTEXT("DroneOps","FleetUX.NeedTarget","Right-click a destination")},
{TEXT("FleetUX.Reselect"),NSLOCTEXT("DroneOps","FleetUX.Reselect","Choose again")},
{TEXT("FleetUX.Exit"),NSLOCTEXT("DroneOps","FleetUX.Exit","Cancel / Esc")},
{TEXT("FleetUX.Hide"),NSLOCTEXT("DroneOps","FleetUX.Hide","Collapse")},
{TEXT("FleetUX.Coordinates"),NSLOCTEXT("DroneOps","FleetUX.Coordinates","Coordinates")},
{TEXT("FleetUX.HideCoordinates"),NSLOCTEXT("DroneOps","FleetUX.HideCoordinates","Hide details")},
{TEXT("FleetUX.Fit"),NSLOCTEXT("DroneOps","FleetUX.Fit","Fit view")},
{TEXT("FleetUX.Preview"),NSLOCTEXT("DroneOps","FleetUX.Preview","DESTINATION PREVIEW")},
{TEXT("FleetUX.Formation"),NSLOCTEXT("DroneOps","FleetUX.Formation","Keep formation and height offsets.")},
{TEXT("FleetUX.HeightHint"),NSLOCTEXT("DroneOps","FleetUX.HeightHint","Drag to adjust, or enter exact ellipsoid height.")},
{TEXT("FleetUX.SliderHint"),NSLOCTEXT("DroneOps","FleetUX.SliderHint","Fine adjustment: ±100 m from the current anchor height.")},
{TEXT("FleetUX.Rise"),NSLOCTEXT("DroneOps","FleetUX.Rise","Current {0} m · rise {1} m")},
{TEXT("FleetUX.Descend"),NSLOCTEXT("DroneOps","FleetUX.Descend","Current {0} m · descend {1} m")},
{TEXT("FleetUX.Level"),NSLOCTEXT("DroneOps","FleetUX.Level","Current {0} m · keep height")},
{TEXT("FleetUX.Selected"),NSLOCTEXT("DroneOps","FleetUX.Selected","{0} aircraft · anchor {1}")},
{TEXT("FleetUX.EmptySelection"),NSLOCTEXT("DroneOps","FleetUX.EmptySelection","No aircraft selected")},
{TEXT("FleetUX.SelectHint"),NSLOCTEXT("DroneOps","FleetUX.SelectHint","Left-click an aircraft or drag to select. Shift adds; Ctrl toggles.")},
{TEXT("FleetUX.SelectionReady"),NSLOCTEXT("DroneOps","FleetUX.SelectionReady","Selection ready. Right-click the map to preview a move.")},
{TEXT("FleetUX.PickHint"),NSLOCTEXT("DroneOps","FleetUX.PickHint","Right-click a destination. Left-drag to change aircraft.")},
{TEXT("FleetUX.ReviewHint"),NSLOCTEXT("DroneOps","FleetUX.ReviewHint","Review height and formation.")},
{TEXT("FleetUX.RunningHint"),NSLOCTEXT("DroneOps","FleetUX.RunningHint","The group moves together. Control it here.")},
{TEXT("FleetUX.FinishedHint"),NSLOCTEXT("DroneOps","FleetUX.FinishedHint","Movement ended. You can choose a new target.")},
{TEXT("FleetUX.State"),NSLOCTEXT("DroneOps","FleetUX.State","{0} · {1}")},
{TEXT("FleetUX.Travel"),NSLOCTEXT("DroneOps","FleetUX.Travel","≈ {0} m  ·  ≈ {1} s")},
{TEXT("FleetUX.Distance"),NSLOCTEXT("DroneOps","FleetUX.Distance","≈ {0} m")},
{TEXT("FleetUX.TargetCoordinates"),NSLOCTEXT("DroneOps","FleetUX.TargetCoordinates","Latitude {0}\nLongitude {1}\nEllipsoid height {2} m")},
{TEXT("FleetUX.Marker"),NSLOCTEXT("DroneOps","FleetUX.Marker","UAV-{0}  ·  {1} m")},
{TEXT("FleetUX.AbortHint"),NSLOCTEXT("DroneOps","FleetUX.AbortHint","Stop the whole group? This movement cannot be resumed.")},
{TEXT("FleetUX.Keep"),NSLOCTEXT("DroneOps","FleetUX.Keep","Keep task")},
{TEXT("FleetUX.AbortConfirm"),NSLOCTEXT("DroneOps","FleetUX.AbortConfirm","Stop group")},
{TEXT("FleetUX.Accepted"),NSLOCTEXT("DroneOps","FleetUX.Accepted","Action confirmed.")},
{TEXT("FleetUX.SyncMock"),NSLOCTEXT("DroneOps","FleetUX.SyncMock","Synced · Simulation")},
{TEXT("FleetUX.SyncReady"),NSLOCTEXT("DroneOps","FleetUX.SyncReady","Synced · Connected")},
{TEXT("FleetUX.SyncWaiting"),NSLOCTEXT("DroneOps","FleetUX.SyncWaiting","Connecting to control service…")},
{TEXT("FleetUX.PreviewOnly"),NSLOCTEXT("DroneOps","FleetUX.PreviewOnly","Preview only · not moving")},
{TEXT("FleetTerrain.AGL"),NSLOCTEXT("DroneOps","FleetTerrain.AGL","Target height above surface (m)")},
{TEXT("FleetTerrain.UseAbsolute"),NSLOCTEXT("DroneOps","FleetTerrain.UseAbsolute","Advanced: ellipsoid height")},
{TEXT("FleetTerrain.UseAGL"),NSLOCTEXT("DroneOps","FleetTerrain.UseAGL","Use height above surface")},
{TEXT("FleetTerrain.HeightHint"),NSLOCTEXT("DroneOps","FleetTerrain.HeightHint","Height above surface uses the loaded map surface as its reference; ellipsoid height is absolute. Load the surface before confirming.")},
{TEXT("FleetTerrain.Datum"),NSLOCTEXT("DroneOps","FleetTerrain.Datum","Surface {0} m ellipsoid · current {1} m above surface\nTarget {2} m ellipsoid")},
{TEXT("FleetTerrain.SourceBelow"),NSLOCTEXT("DroneOps","FleetTerrain.SourceBelow","Selected aircraft are below the loaded surface or have less than 1 m clearance. Check the altitude datum; Mock aircraft can be raised before moving.")},
{TEXT("FleetTerrain.TooLow"),NSLOCTEXT("DroneOps","FleetTerrain.TooLow","The selected formation intersects the loaded surface. Increase the height or choose another destination.")},
{TEXT("FleetTerrain.Recover"),NSLOCTEXT("DroneOps","FleetTerrain.Recover","Raise Mock fleet above surface")},
{TEXT("FleetTerrain.RecoveryPreview"),NSLOCTEXT("DroneOps","FleetTerrain.RecoveryPreview","Vertical Mock recovery preview only. Confirm to raise the group to at least 30 m above the surface.")},
{TEXT("FleetTerrain.InvalidHeight"),NSLOCTEXT("DroneOps","FleetTerrain.InvalidHeight","Enter a finite height within 10 km of the anchor; surface-relative height must be at least 1 m.")},
{TEXT("FleetTerrain.MarkerAGL"),NSLOCTEXT("DroneOps","FleetTerrain.MarkerAGL","UAV-{0} · {1} m AGL")},
{TEXT("FleetTerrain.TargetAGL"),NSLOCTEXT("DroneOps","FleetTerrain.TargetAGL","Target · {0} m AGL")},
{TEXT("Configuration.NewPlan"),NSLOCTEXT("DroneOps","Configuration.NewPlan","New security plan")},
{TEXT("Configuration.MapContext"),NSLOCTEXT("DroneOps","Configuration.MapContext","Configuring {0}. Continue configuration in Command.")},
{TEXT("Configuration.MoveBlocked"),NSLOCTEXT("DroneOps","Configuration.MoveBlocked","Finish plan configuration before starting a fleet move.")},
{TEXT("ExecutionControls.Title"),NSLOCTEXT("DroneOps","ExecutionControls.Title","Active execution controls")},
{TEXT("ExecutionControls.Group"),NSLOCTEXT("DroneOps","ExecutionControls.Group","{0} · {1} | {2}\n{3} · {4}")},
{TEXT("ExecutionControls.ConfirmAbort"),NSLOCTEXT("DroneOps","ExecutionControls.ConfirmAbort","Confirm stop")},
{TEXT("ExecutionControls.Result"),NSLOCTEXT("DroneOps","ExecutionControls.Result","{0}: {1}")},
{TEXT("Execution.Mixed"),NSLOCTEXT("DroneOps","Execution.Mixed","Mixed member states")},
{TEXT("Configuration.Context"),NSLOCTEXT("DroneOps","Configuration.Context","Configuring {0} · {1} | {2}")},
{TEXT("Configuration.OfflineDraft"),NSLOCTEXT("DroneOps","Configuration.OfflineDraft","Offline · local changes retained")},
{TEXT("Configuration.ReadOnly"),NSLOCTEXT("DroneOps","Configuration.ReadOnly","Read only · enter configuration to edit")},
{TEXT("Configuration.Unsaved"),NSLOCTEXT("DroneOps","Configuration.Unsaved","Unsaved changes")},
{TEXT("Configuration.Saved"),NSLOCTEXT("DroneOps","Configuration.Saved","Saved")},
{TEXT("Configuration.NotCreated"),NSLOCTEXT("DroneOps","Configuration.NotCreated","Not created yet")},
{TEXT("Configuration.UnsavedPrompt"),NSLOCTEXT("DroneOps","Configuration.UnsavedPrompt","You have unsaved changes. Save and continue, discard these changes, or keep editing.")},
{TEXT("Configuration.SaveContinue"),NSLOCTEXT("DroneOps","Configuration.SaveContinue","Save and continue")},
{TEXT("Configuration.DiscardContinue"),NSLOCTEXT("DroneOps","Configuration.DiscardContinue","Discard changes and continue")},
{TEXT("Configuration.KeepEditing"),NSLOCTEXT("DroneOps","Configuration.KeepEditing","Keep editing")},
{TEXT("Configuration.Resume"),NSLOCTEXT("DroneOps","Configuration.Resume","Continue configuration")},
{TEXT("Configuration.Finish"),NSLOCTEXT("DroneOps","Configuration.Finish","Finish configuration")},
{TEXT("Configuration.NewMissionName"),NSLOCTEXT("DroneOps","Configuration.NewMissionName","Task {0}")},
{TEXT("Configuration.Return"),NSLOCTEXT("DroneOps","Configuration.Return","Return to configuration")},
{TEXT("Configuration.ScheduledWaiting"),NSLOCTEXT("DroneOps","Configuration.ScheduledWaiting","Scheduled starts wait until configuration is finished.")},
{TEXT("Errors.CONFIGURATION_IN_PROGRESS"),NSLOCTEXT("DroneOps","Errors.CONFIGURATION_IN_PROGRESS","Finish the current plan configuration first. Alerts and active task pause/stop controls remain available.")},
{TEXT("Errors.CONFIGURATION_CONTEXT_CHANGED"),NSLOCTEXT("DroneOps","Errors.CONFIGURATION_CONTEXT_CHANGED","Selection changed elsewhere. Your local changes are retained; finish or discard them before switching.")},
{TEXT("Configuration.MapRouteContext"),NSLOCTEXT("DroneOps","Configuration.MapRouteContext","Editing the route for {0}. Finish the route to continue in Command.")},
{TEXT("Configuration.DeployedReadOnly"),NSLOCTEXT("DroneOps","Configuration.DeployedReadOnly","Deployed plan is read only. Create a new version to make changes.")},
{TEXT("Figma.TotalUAV"),NSLOCTEXT("DroneOps","Figma.TotalUAV","Total UAVs")},
{TEXT("Figma.OnlineDevices"),NSLOCTEXT("DroneOps","Figma.OnlineDevices","Online devices")},
{TEXT("Figma.AlertCenter"),NSLOCTEXT("DroneOps","Figma.AlertCenter","Alert center")},
{TEXT("Figma.CurrentTask"),NSLOCTEXT("DroneOps","Figma.CurrentTask","Current task")},
{TEXT("Figma.CurrentPlan"),NSLOCTEXT("DroneOps","Figma.CurrentPlan","Current plan")},
{TEXT("Figma.ViewAll"),NSLOCTEXT("DroneOps","Figma.ViewAll","View all ›")},
{TEXT("Figma.Close"),NSLOCTEXT("DroneOps","Figma.Close","Close")},
{TEXT("Figma.Refresh"),NSLOCTEXT("DroneOps","Figma.Refresh","Refresh")},
{TEXT("Figma.RoleHealth"),NSLOCTEXT("DroneOps","Figma.RoleHealth","{0} {1}")},
{TEXT("Figma.ConfigurationBusy"),NSLOCTEXT("DroneOps","Figma.ConfigurationBusy","Finish the current configuration before opening the fleet.")},
{TEXT("Figma.DeviceId"),NSLOCTEXT("DroneOps","Figma.DeviceId","Device ID")},
{TEXT("Figma.DeviceName"),NSLOCTEXT("DroneOps","Figma.DeviceName","Device name")},
{TEXT("Figma.Connection"),NSLOCTEXT("DroneOps","Figma.Connection","Connection")},
{TEXT("Figma.Battery"),NSLOCTEXT("DroneOps","Figma.Battery","Battery")},
{TEXT("Figma.Actions"),NSLOCTEXT("DroneOps","Figma.Actions","Actions")},
{TEXT("Figma.TimeUTC"),NSLOCTEXT("DroneOps","Figma.TimeUTC","Time (UTC)")},
{TEXT("Figma.AlertType"),NSLOCTEXT("DroneOps","Figma.AlertType","Alert type")},
{TEXT("Figma.EventType"),NSLOCTEXT("DroneOps","Figma.EventType","Type")},
{TEXT("Figma.Content"),NSLOCTEXT("DroneOps","Figma.Content","Content")},
{TEXT("Figma.Details"),NSLOCTEXT("DroneOps","Figma.Details","View details")},
{TEXT("Figma.NoDevices"),NSLOCTEXT("DroneOps","Figma.NoDevices","No registered devices")},
{TEXT("Figma.DeviceCount"),NSLOCTEXT("DroneOps","Figma.DeviceCount","{0} devices · Includes offline devices")},
{TEXT("Figma.AlertCount"),NSLOCTEXT("DroneOps","Figma.AlertCount","{0} alerts")},
{TEXT("Figma.LogCount"),NSLOCTEXT("DroneOps","Figma.LogCount","{0} records")},
{TEXT("Figma.WaitingPosition"),NSLOCTEXT("DroneOps","Figma.WaitingPosition","Waiting for UAV location data")},
{TEXT("Figma.Plans.EmptyTitle"),NSLOCTEXT("DroneOps","Figma.Plans.EmptyTitle","No security plans yet")},
{TEXT("Figma.Plans.EmptyHint"),NSLOCTEXT("DroneOps","Figma.Plans.EmptyHint","Create a plan to configure devices and mission routes")},
{TEXT("Figma.Plans.NewPlan"),NSLOCTEXT("DroneOps","Figma.Plans.NewPlan","New security plan")},
{TEXT("Figma.Plans.NewPlanHint"),NSLOCTEXT("DroneOps","Figma.Plans.NewPlanHint","Enter basic information to continue configuring your security plan")},
{TEXT("Figma.Plans.NameHint"),NSLOCTEXT("DroneOps","Figma.Plans.NameHint","Enter a plan name")},
{TEXT("Figma.Plans.DescriptionHint"),NSLOCTEXT("DroneOps","Figma.Plans.DescriptionHint","Enter a plan description")},
{TEXT("Figma.Plans.Flow"),NSLOCTEXT("DroneOps","Figma.Plans.Flow","Plan workflow: Create / Configure devices / Edit routes / Review / Deploy")},
{TEXT("Figma.Fleet.Title"),NSLOCTEXT("DroneOps","Figma.Fleet.Title","UAVs")},
{TEXT("Figma.Fleet.List"),NSLOCTEXT("DroneOps","Figma.Fleet.List","UAV list")},
{TEXT("Figma.Fleet.Empty"),NSLOCTEXT("DroneOps","Figma.Fleet.Empty","No registered UAVs")},
{TEXT("Figma.Fleet.Telemetry"),NSLOCTEXT("DroneOps","Figma.Fleet.Telemetry","Current UAV telemetry")},
{TEXT("Figma.Fleet.Altitude"),NSLOCTEXT("DroneOps","Figma.Fleet.Altitude","Altitude")},
{TEXT("Figma.Fleet.Speed"),NSLOCTEXT("DroneOps","Figma.Fleet.Speed","Speed")},
{TEXT("Figma.Fleet.Battery"),NSLOCTEXT("DroneOps","Figma.Fleet.Battery","Battery")},
{TEXT("Figma.Fleet.Link"),NSLOCTEXT("DroneOps","Figma.Fleet.Link","Link")},
{TEXT("Figma.Fleet.Task"),NSLOCTEXT("DroneOps","Figma.Fleet.Task","Task")},
{TEXT("Figma.Fleet.Group"),NSLOCTEXT("DroneOps","Figma.Fleet.Group","UAV group control")},
{TEXT("Figma.Fleet.Video"),NSLOCTEXT("DroneOps","Figma.Fleet.Video","Video monitor")},
{TEXT("Figma.Fleet.NoVideo"),NSLOCTEXT("DroneOps","Figma.Fleet.NoVideo","No video signal")},
{TEXT("Figma.Fleet.VideoHint"),NSLOCTEXT("DroneOps","Figma.Fleet.VideoHint","Choose a UAV, then open it in the Video client")},
{TEXT("Figma.Fleet.VideoTarget"),NSLOCTEXT("DroneOps","Figma.Fleet.VideoTarget","Video client target: {0}")},
{TEXT("Figma.Fleet.Join"),NSLOCTEXT("DroneOps","Figma.Fleet.Join","Add to group")},
{TEXT("Figma.Fleet.SetPrimary"),NSLOCTEXT("DroneOps","Figma.Fleet.SetPrimary","Set primary")},
{TEXT("Figma.Fleet.Label"),NSLOCTEXT("DroneOps","Figma.Fleet.Label","Edit label")},
{TEXT("Figma.Fleet.RowDetail"),NSLOCTEXT("DroneOps","Figma.Fleet.RowDetail","Battery {0}  ·  Task {1}")},
{TEXT("Figma.Fleet.NoSelection"),NSLOCTEXT("DroneOps","Figma.Fleet.NoSelection","No UAV selected")},
{TEXT("Figma.Fleet.LabelTitle"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelTitle","Edit UAV label")},
{TEXT("Figma.Fleet.LabelName"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelName","Display name")},
{TEXT("Figma.Fleet.LabelColor"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelColor","Label color")},
{TEXT("Figma.Fleet.LabelSize"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelSize","Font size")},
{TEXT("Figma.Fleet.LabelScope"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelScope","Only affects the UAV label")},
{TEXT("Figma.Fleet.LabelPreview"),NSLOCTEXT("DroneOps","Figma.Fleet.LabelPreview","Label preview")},
{TEXT("Figma.Fleet.Confirm"),NSLOCTEXT("DroneOps","Figma.Fleet.Confirm","Confirm")},
{TEXT("Figma.Fleet.Task.Assembling"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Assembling","Assembling")},
{TEXT("Figma.Fleet.Task.Moving"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Moving","Moving")},
{TEXT("Figma.Fleet.Task.Scouting"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Scouting","Scouting")},
{TEXT("Figma.Fleet.Task.Patrolling"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Patrolling","Patrolling")},
{TEXT("Figma.Fleet.Task.Attacking"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Attacking","Attacking")},
{TEXT("Figma.Fleet.Task.Paused"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Paused","Paused")},
{TEXT("Figma.Fleet.Task.Avoiding"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Avoiding","Avoiding")},
{TEXT("Figma.Fleet.Task.Completed"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Completed","Completed")},
{TEXT("Figma.Fleet.Task.Error"),NSLOCTEXT("DroneOps","Figma.Fleet.Task.Error","Error")},
{TEXT("Figma.Fleet.VideoOffline"),NSLOCTEXT("DroneOps","Figma.Fleet.VideoOffline","This UAV is offline. Check its connection.")},
{TEXT("Figma.Fleet.VideoClient"),NSLOCTEXT("DroneOps","Figma.Fleet.VideoClient","View in Video client")},
{TEXT("Figma.Fleet.VideoTargetHint"),NSLOCTEXT("DroneOps","Figma.Fleet.VideoTargetHint","Current target: {0}. Use the upper-right button to view it in the Video client.")},
{TEXT("Figma.Fleet.SelectionSuperseded"),NSLOCTEXT("DroneOps","Figma.Fleet.SelectionSuperseded","Selection changed in another operation. Please retry.")},
{TEXT("Overview.ActiveTasks"),NSLOCTEXT("DroneOps","Overview.ActiveTasks","Active tasks ({0})")},
{TEXT("Overview.ViewTask"),NSLOCTEXT("DroneOps","Overview.ViewTask","View task")},
{TEXT("Overview.FreeMove"),NSLOCTEXT("DroneOps","Overview.FreeMove","Free movement")},
{TEXT("Overview.TaskState"),NSLOCTEXT("DroneOps","Overview.TaskState","{0} · {1} · {2}")},
{TEXT("Overview.TaskMembers"),NSLOCTEXT("DroneOps","Overview.TaskMembers","{0} · {1}")},
{TEXT("Plan.Tasks.Title"),NSLOCTEXT("DroneOps","Plan.Tasks.Title","Tasks in this plan")},
{TEXT("Plan.Tasks.Hint"),NSLOCTEXT("DroneOps","Plan.Tasks.Hint","Choose a task to edit or execute. Checking and deploying a plan does not start a task.")},
{TEXT("Plan.Tasks.Assignment"),NSLOCTEXT("DroneOps","Plan.Tasks.Assignment","UAVs: {0} · Waypoints: {1}")},
{TEXT("Plan.Tasks.State"),NSLOCTEXT("DroneOps","Plan.Tasks.State","Configuration: {0} · Execution: {1}")},
{TEXT("Plan.Tasks.Select"),NSLOCTEXT("DroneOps","Plan.Tasks.Select","Select task")},
{TEXT("Plan.Tasks.Selected"),NSLOCTEXT("DroneOps","Plan.Tasks.Selected","Selected")},
{TEXT("Plan.Tasks.Edit"),NSLOCTEXT("DroneOps","Plan.Tasks.Edit","Edit task")},
{TEXT("Plan.Tasks.EditVersion"),NSLOCTEXT("DroneOps","Plan.Tasks.EditVersion","Edit in new version")},
{TEXT("Plan.Tasks.Execute"),NSLOCTEXT("DroneOps","Plan.Tasks.Execute","Execute task")},
{TEXT("Plan.Tasks.CheckDeploy"),NSLOCTEXT("DroneOps","Plan.Tasks.CheckDeploy","Check and deploy")},
{TEXT("Plan.Tasks.ViewExecution"),NSLOCTEXT("DroneOps","Plan.Tasks.ViewExecution","View execution")},
{TEXT("MapMove.Title"),NSLOCTEXT("DroneOps","MapMove.Title","Map movement")},
{TEXT("MapMove.SelectAircraft"),NSLOCTEXT("DroneOps","MapMove.SelectAircraft","Select at least one aircraft to move.")},
{TEXT("MapMove.SelectHint"),NSLOCTEXT("DroneOps","MapMove.SelectHint","Select an aircraft icon. Shift adds aircraft; Ctrl toggles selection.")},
{TEXT("MapMove.ClickHint"),NSLOCTEXT("DroneOps","MapMove.ClickHint","{0} selected · Click a map destination to preview. Drag to pan; wheel to zoom.")},
{TEXT("MapMove.Preview"),NSLOCTEXT("DroneOps","MapMove.Preview","Move {0} aircraft to {1}, {2} · altitude {3} m. Confirm to send; click the map to choose again.")},
{TEXT("MapMove.Confirm"),NSLOCTEXT("DroneOps","MapMove.Confirm","Confirm movement")},
{TEXT("MapMove.EditCameraHint"),NSLOCTEXT("DroneOps","MapMove.EditCameraHint","Edit view: middle drag to pan, right drag to orbit in 3D, wheel to zoom. Left mouse edits waypoints.")},
{TEXT("MapMove.MockMarker"),NSLOCTEXT("DroneOps","MapMove.MockMarker","{0} · Mock")},
{TEXT("Figma.Header.BackendOnline"),NSLOCTEXT("DroneOps","Figma.Header.BackendOnline","Backend online")},
{TEXT("Figma.Header.BackendOffline"),NSLOCTEXT("DroneOps","Figma.Header.BackendOffline","Backend offline")},
{TEXT("Figma.Header.Synced"),NSLOCTEXT("DroneOps","Figma.Header.Synced","Synchronized")},
{TEXT("Figma.Header.SyncPending"),NSLOCTEXT("DroneOps","Figma.Header.SyncPending","Sync pending")},
{TEXT("VideoUI.Title"),NSLOCTEXT("DroneOps","VideoUI.Title","Video intelligence")},
{TEXT("VideoUI.Single"),NSLOCTEXT("DroneOps","VideoUI.Single","One active player · Other slots reserved · AI not connected")},
{TEXT("VideoUI.Aircraft"),NSLOCTEXT("DroneOps","VideoUI.Aircraft","Aircraft")},
{TEXT("VideoUI.SelectSource"),NSLOCTEXT("DroneOps","VideoUI.SelectSource","Select a video target")},
{TEXT("VideoUI.ModelDisclaimer"),NSLOCTEXT("DroneOps","VideoUI.ModelDisclaimer","Visual presets do not represent aircraft specifications.")},
{TEXT("VideoUI.Primary"),NSLOCTEXT("DroneOps","VideoUI.Primary","Primary video")},
{TEXT("VideoUI.Current"),NSLOCTEXT("DroneOps","VideoUI.Current","Video target")},
{TEXT("VideoUI.Detection"),NSLOCTEXT("DroneOps","VideoUI.Detection","AI detection")},
{TEXT("VideoUI.People"),NSLOCTEXT("DroneOps","VideoUI.People","People")},
{TEXT("VideoUI.Vehicles"),NSLOCTEXT("DroneOps","VideoUI.Vehicles","Vehicles")},
{TEXT("VideoUI.Drones"),NSLOCTEXT("DroneOps","VideoUI.Drones","UAVs")},
{TEXT("VideoUI.Behavior"),NSLOCTEXT("DroneOps","VideoUI.Behavior","Abnormal behavior")},
{TEXT("VideoUI.Other"),NSLOCTEXT("DroneOps","VideoUI.Other","Other targets")},
{TEXT("VideoUI.Alerts"),NSLOCTEXT("DroneOps","VideoUI.Alerts","Recent alerts")},
{TEXT("VideoUI.NoAlerts"),NSLOCTEXT("DroneOps","VideoUI.NoAlerts","No active alerts")},
{TEXT("VideoUI.AlertSession"),NSLOCTEXT("DroneOps","VideoUI.AlertSession","Received alerts · This session")},
{TEXT("VideoUI.System"),NSLOCTEXT("DroneOps","VideoUI.System","System status")},
{TEXT("VideoUI.Online"),NSLOCTEXT("DroneOps","VideoUI.Online","Online aircraft")},
{TEXT("VideoUI.AI"),NSLOCTEXT("DroneOps","VideoUI.AI","AI engine")},
{TEXT("VideoUI.Storage"),NSLOCTEXT("DroneOps","VideoUI.Storage","Storage")},
{TEXT("VideoUI.Network"),NSLOCTEXT("DroneOps","VideoUI.Network","Backend")},
{TEXT("VideoUI.NotConnected"),NSLOCTEXT("DroneOps","VideoUI.NotConnected","Not connected")},
{TEXT("VideoUI.Unavailable"),NSLOCTEXT("DroneOps","VideoUI.Unavailable","Unavailable")},
{TEXT("VideoUI.Four"),NSLOCTEXT("DroneOps","VideoUI.Four","4 slots")},
{TEXT("VideoUI.Six"),NSLOCTEXT("DroneOps","VideoUI.Six","6 slots")},
{TEXT("VideoUI.Focus"),NSLOCTEXT("DroneOps","VideoUI.Focus","Focus")},
{TEXT("VideoUI.SetActive"),NSLOCTEXT("DroneOps","VideoUI.SetActive","Select this UAV")},
{TEXT("VideoUI.SetActiveHint"),NSLOCTEXT("DroneOps","VideoUI.SetActiveHint","Explicitly select this UAV in the shared context. This action does not change the video target.")},
{TEXT("VideoUI.ActiveSelection"),NSLOCTEXT("DroneOps","VideoUI.ActiveSelection","Selected: {0}")},
{TEXT("VideoUI.Retry"),NSLOCTEXT("DroneOps","VideoUI.Retry","Retry")},
{TEXT("VideoUI.Record"),NSLOCTEXT("DroneOps","VideoUI.Record","Record")},
{TEXT("VideoUI.Playback"),NSLOCTEXT("DroneOps","VideoUI.Playback","Playback")},
{TEXT("VideoUI.Screenshot"),NSLOCTEXT("DroneOps","VideoUI.Screenshot","Capture")},
{TEXT("VideoUI.Filter"),NSLOCTEXT("DroneOps","VideoUI.Filter","Filter")},
{TEXT("VideoUI.Unsupported"),NSLOCTEXT("DroneOps","VideoUI.Unsupported","This capability is not connected yet.")},
{TEXT("VideoUI.Reserved"),NSLOCTEXT("DroneOps","VideoUI.Reserved","Reserved slot")},
{TEXT("VideoUI.Unassigned"),NSLOCTEXT("DroneOps","VideoUI.Unassigned","No UAV assigned")},
{TEXT("VideoUI.ModelUnavailable"),NSLOCTEXT("DroneOps","VideoUI.ModelUnavailable","Model: unavailable")},
{TEXT("VideoUI.NoSource"),NSLOCTEXT("DroneOps","VideoUI.NoSource","No video source")},
{TEXT("VideoUI.InactiveSource"),NSLOCTEXT("DroneOps","VideoUI.InactiveSource","Click to view")},
{TEXT("VideoUI.SourceSet"),NSLOCTEXT("DroneOps","VideoUI.SourceSet","Source configured")},
{TEXT("VideoUI.NoTarget"),NSLOCTEXT("DroneOps","VideoUI.NoTarget","No video target")},
{TEXT("VideoUI.ConfigureSource"),NSLOCTEXT("DroneOps","VideoUI.ConfigureSource","Configure a video source for this UAV")},
{TEXT("VideoUI.WaitingFrames"),NSLOCTEXT("DroneOps","VideoUI.WaitingFrames","Waiting for decoded video frames")},
{TEXT("VideoUI.RetryHint"),NSLOCTEXT("DroneOps","VideoUI.RetryHint","Reconnect the current video source")},
{TEXT("VideoUI.RetryUnavailable"),NSLOCTEXT("DroneOps","VideoUI.RetryUnavailable","Select a UAV with a configured video source to retry.")},
{TEXT("VideoUI.NA"),NSLOCTEXT("DroneOps","VideoUI.NA","N/A")},
{TEXT("MapUI.Operations"),NSLOCTEXT("DroneOps","MapUI.Operations","Map operations")},
{TEXT("MapUI.MissionRoute"),NSLOCTEXT("DroneOps","MapUI.MissionRoute","Mission route")},
{TEXT("MapUI.ViewVideo"),NSLOCTEXT("DroneOps","MapUI.ViewVideo","View video")},
{TEXT("MapUI.CurrentDevice"),NSLOCTEXT("DroneOps","MapUI.CurrentDevice","Current device")},
{TEXT("MapUI.SelectDevice"),NSLOCTEXT("DroneOps","MapUI.SelectDevice","Select a device")},
{TEXT("MapUI.CameraHint"),NSLOCTEXT("DroneOps","MapUI.CameraHint","Middle: pan · Right: orbit · Wheel: zoom")},
{TEXT("MapUI.FleetPointerHint"),NSLOCTEXT("DroneOps","MapUI.FleetPointerHint","Left: select · Right-click: target · Middle: pan")},
{TEXT("MapUI.FleetPointerHelp"),NSLOCTEXT("DroneOps","MapUI.FleetPointerHelp","Left-click or drag: select aircraft. Right-click: preview destination, then confirm. Middle-drag: pan. Right-drag: pan in 2D, orbit in 3D. Wheel: zoom.")},
{TEXT("MapUI.RouteEditor"),NSLOCTEXT("DroneOps","MapUI.RouteEditor","Route editor")},
{TEXT("MapUI.DefaultSpeed"),NSLOCTEXT("DroneOps","MapUI.DefaultSpeed","Default flight speed")},
{TEXT("MapUI.DefaultSpeedHint"),NSLOCTEXT("DroneOps","MapUI.DefaultSpeedHint","Used for newly added waypoints.")},
{TEXT("MapUI.SpeedRange"),NSLOCTEXT("DroneOps","MapUI.SpeedRange","m/s · 0 = unlimited, or 0.5–15")},
{TEXT("MapUI.SpeedInvalid"),NSLOCTEXT("DroneOps","MapUI.SpeedInvalid","Enter 0 (unlimited) or a speed from 0.5 to 15 m/s.")},
{TEXT("Plan.CopyExplicit"),NSLOCTEXT("DroneOps","Plan.CopyExplicit","Copy plan")},
{TEXT("Plan.EditExisting"),NSLOCTEXT("DroneOps","Plan.EditExisting","Edit plan")},
{TEXT("Plan.EditDeployedHint"),NSLOCTEXT("DroneOps","Plan.EditDeployedHint","This plan is deployed. Edit this plan to save changes under the same ID, then validate and deploy again. Previous deployments remain in history.")},
{TEXT("Plan.Delete.Action"),NSLOCTEXT("DroneOps","Plan.Delete.Action","Delete plan")},
{TEXT("Plan.Delete.Confirm"),NSLOCTEXT("DroneOps","Plan.Delete.Confirm","Confirm deletion")},
{TEXT("Plan.Delete.Refresh"),NSLOCTEXT("DroneOps","Plan.Delete.Refresh","Review latest version")},
{TEXT("Plan.Delete.NameHint"),NSLOCTEXT("DroneOps","Plan.Delete.NameHint","Enter the exact plan name")},
{TEXT("Plan.Delete.Prompt"),NSLOCTEXT("DroneOps","Plan.Delete.Prompt","Delete “{0}” from the plan list? Enter the exact name below to confirm. Current tasks and routes will be archived; deployment and execution history will be retained. Active or scheduled tasks must be finished or cancelled first.")},
{TEXT("Errors.DELETE_CONFIRMATION_REQUIRED"),NSLOCTEXT("DroneOps","Errors.DELETE_CONFIRMATION_REQUIRED","The confirmation name does not match this plan. Review the latest version and enter its exact name again.")},
{TEXT("Errors.CONTENT_REVISION_CONFLICT"),NSLOCTEXT("DroneOps","Errors.CONTENT_REVISION_CONFLICT","The plan changed after you opened this action. Review the latest version and confirm again.")},
{TEXT("Errors.CONFIGURATION_REQUIRED"),NSLOCTEXT("DroneOps","Errors.CONFIGURATION_REQUIRED","Open this plan for configuration before editing it.")},
{TEXT("Errors.PLAN_NOT_FOUND"),NSLOCTEXT("DroneOps","Errors.PLAN_NOT_FOUND","This plan no longer exists. Close this dialog and return to the plan list.")},
{TEXT("Errors.PLAN_NOT_DEPLOYED"),NSLOCTEXT("DroneOps","Errors.PLAN_NOT_DEPLOYED","This plan's deployment state changed. Reload the plan and retry.")},
{TEXT("Events.PLAN_REOPENED"),NSLOCTEXT("DroneOps","Events.PLAN_REOPENED","Plan {plan_name} reopened for editing")},
{TEXT("FleetUX.ExecutionControls"),NSLOCTEXT("DroneOps","FleetUX.ExecutionControls","Execution controls")},
};return C;}
}
FText ProductText::Get(const FString& Key){if(const auto* T=Catalog().Find(Key))return *T;return NSLOCTEXT("DroneOps","Errors.Request","Request failed. Confirm the current selection and retry.");}
FText ProductText::Source(const FString& Source){
if(Source==TEXT("SECURITY PLAN"))return Get(TEXT("Plan.Title"));
if(Source==TEXT("Plan name"))return Get(TEXT("Plan.Name"));
if(Source==TEXT("Description"))return Get(TEXT("Plan.Description"));
if(Source==TEXT("NEW PLAN"))return Get(TEXT("Plan.New"));
if(Source==TEXT("SAVE PLAN INFORMATION"))return Get(TEXT("Plan.Update"));
if(Source==TEXT("Task"))return Get(TEXT("Plan.Mission"));
if(Source==TEXT("MISSIONS"))return Get(TEXT("Plan.Missions"));
if(Source==TEXT("Task name"))return Get(TEXT("Plan.MissionName"));
if(Source==TEXT("ADD TASK"))return Get(TEXT("Plan.AddMission"));
if(Source==TEXT("RENAME TASK"))return Get(TEXT("Plan.Rename"));
if(Source==TEXT("DELETE MISSION"))return Get(TEXT("Plan.Delete"));
if(Source==TEXT("ASSIGN UAV"))return Get(TEXT("Plan.Assign"));
if(Source==TEXT("UNASSIGN UAV"))return Get(TEXT("Plan.Unassign"));
if(Source==TEXT("No missions / Add first mission"))return Get(TEXT("Plan.NoMissions"));
if(Source==TEXT("No active plan"))return Get(TEXT("Plan.NoPlan"));
if(Source==TEXT("EDIT ROUTE ON MAP"))return Get(TEXT("Plan.EditMap"));
if(Source==TEXT("CHECK CONFIGURATION"))return Get(TEXT("Plan.Check"));
if(Source==TEXT("REVIEW PLAN"))return Get(TEXT("Plan.Review"));
if(Source==TEXT("CONFIRM REVIEW"))return Get(TEXT("Plan.ConfirmReview"));
if(Source==TEXT("DEPLOY PLAN"))return Get(TEXT("Plan.Deploy"));
if(Source==TEXT("CONFIRM DEPLOY"))return Get(TEXT("Plan.ConfirmDeploy"));
if(Source==TEXT("COPY AS DRAFT"))return Get(TEXT("Plan.Copy"));
if(Source==TEXT("DRAFT"))return Get(TEXT("Plan.DRAFT"));
if(Source==TEXT("CONFIGURATION VALID"))return Get(TEXT("Plan.READY"));
if(Source==TEXT("Deployed"))return Get(TEXT("Plan.DEPLOYED"));
if(Source==TEXT("This review confirms the current plan configuration. Deployment freezes the business configuration and does not start aircraft execution."))return Get(TEXT("Plan.ReviewNotice"));
if(Source==TEXT("Revision {0}"))return Get(TEXT("Plan.Revision"));
if(Source==TEXT("Reviewed revision {0}"))return Get(TEXT("Plan.Reviewed"));
if(Source==TEXT("{0} | UAV: {1} | Waypoints: {2}"))return Get(TEXT("Plan.MissionSummary"));
if(Source==TEXT("Tasks: {0} | UAV assigned: {1} | Routes configured: {2}"))return Get(TEXT("Plan.Counts"));
if(Source==TEXT("Closed route: {0} | Execution: once | Speed: {1} | Wait: {2}"))return Get(TEXT("Plan.RouteParameters"));
if(Source==TEXT("Blocking issues"))return Get(TEXT("Plan.Blocking"));
if(Source==TEXT("Configuration check only. Terrain, airspace, endurance and flight safety are not assessed."))return Get(TEXT("Plan.Warnings"));
if(Source==TEXT("Deployment record"))return Get(TEXT("Plan.Deployment"));
if(Source==TEXT("SAVE"))return Get(TEXT("Common.Save"));
if(Source==TEXT("CANCEL"))return Get(TEXT("Common.Cancel"));
if(Source==TEXT("DISCARD CHANGES"))return Get(TEXT("Common.Discard"));
if(Source==TEXT("EXIT"))return Get(TEXT("Common.Exit"));
if(Source==TEXT("Not assigned"))return Get(TEXT("Common.None"));
if(Source==TEXT("Waiting for Backend confirmation..."))return Get(TEXT("Common.Waiting"));
if(Source==TEXT("Backend confirmed"))return Get(TEXT("Common.Confirmed"));
if(Source==TEXT("Language"))return Get(TEXT("Common.Language"));
if(Source==TEXT("MISSION ROUTE"))return Get(TEXT("Map.Route"));
if(Source==TEXT("BEGIN ROUTE EDIT"))return Get(TEXT("Map.Begin"));
if(Source==TEXT("ADD WAYPOINT"))return Get(TEXT("Map.Add"));
if(Source==TEXT("DELETE WAYPOINT"))return Get(TEXT("Map.Delete"));
if(Source==TEXT("CLEAR ROUTE"))return Get(TEXT("Map.Clear"));
if(Source==TEXT("FOCUS ROUTE"))return Get(TEXT("Map.Focus"));
if(Source==TEXT("Unsaved route changes. Save, discard, or cancel before leaving."))return Get(TEXT("Map.DirtyPrompt"));
if(Source==TEXT("Discard local changes and reload the saved route?"))return Get(TEXT("Map.DiscardPrompt"));
if(Source==TEXT("EDITING"))return Get(TEXT("Map.EDITING"));
if(Source==TEXT("DIRTY / UNSAVED"))return Get(TEXT("Map.DIRTY"));
if(Source==TEXT("SAVING"))return Get(TEXT("Map.SAVING"));
if(Source==TEXT("SAVED / READ ONLY"))return Get(TEXT("Map.CLEAN"));
if(Source==TEXT("Waypoints: {0}"))return Get(TEXT("Map.Count"));
if(Source==TEXT("OPEN VIDEO"))return Get(TEXT("Video.Open"));
if(Source==TEXT("VIEW ACTIVE UAV VIDEO"))return Get(TEXT("Video.Active"));
if(Source==TEXT("VIEW ASSIGNED UAV VIDEO"))return Get(TEXT("Video.Assigned"));
if(Source==TEXT("VIDEO TARGET"))return Get(TEXT("Video.Target"));
if(Source==TEXT("Backend offline. No change submitted."))return Get(TEXT("Errors.SYNC_OFFLINE"));
if(Source==TEXT("Synchronization failed. Last confirmed state retained."))return Get(TEXT("Errors.SYNC_FAILED"));
if(Source==TEXT("Map route editing is in progress. Save or discard before review or deployment."))return Get(TEXT("Errors.PLAN_EDIT_IN_PROGRESS"));
if(Source==TEXT("Review the current configuration first."))return Get(TEXT("Errors.PLAN_REVIEW_REQUIRED"));
if(Source==TEXT("Review is stale. Review the current revision again."))return Get(TEXT("Errors.REVIEW_STALE"));
if(Source==TEXT("Configuration validation is stale."))return Get(TEXT("Errors.VALIDATION_STALE"));
if(Source==TEXT("Version conflict. Local draft retained. Discard and reload to resolve."))return Get(TEXT("Errors.VERSION_CONFLICT"));
if(Source==TEXT("Another editor owns this route session."))return Get(TEXT("Errors.EDIT_SESSION_CONFLICT"));
if(Source==TEXT("Editing lease expired. Local draft retained; discard and reload."))return Get(TEXT("Errors.EDIT_SESSION_EXPIRED"));
if(Source==TEXT("Map client unavailable"))return Get(TEXT("Errors.MAP_CLIENT_UNAVAILABLE"));
if(Source==TEXT("Deployed plan is read only."))return Get(TEXT("Errors.PLAN_DEPLOYED"));
if(Source==TEXT("Check configuration and resolve blocking issues."))return Get(TEXT("Errors.PLAN_NOT_READY"));
if(Source==TEXT("Add at least one mission."))return Get(TEXT("Errors.NO_MISSIONS"));
if(Source==TEXT("Assign a UAV to this mission."))return Get(TEXT("Errors.NO_UAV_ASSIGNED"));
if(Source==TEXT("The assigned UAV is not in the Registry."))return Get(TEXT("Errors.INVALID_UAV"));
if(Source==TEXT("This UAV is assigned to another mission in this plan."))return Get(TEXT("Errors.UAV_ALREADY_ASSIGNED"));
if(Source==TEXT("Edit and save a route on Map."))return Get(TEXT("Errors.ROUTE_MISSING"));
if(Source==TEXT("Route requires at least 2 waypoints."))return Get(TEXT("Errors.ROUTE_TOO_SHORT"));
if(Source==TEXT("Waypoint coordinates or parameters are invalid."))return Get(TEXT("Errors.INVALID_WAYPOINT"));
if(Source==TEXT("Enter a valid name."))return Get(TEXT("Errors.INVALID_NAME"));
if(Source==TEXT("Unsupported language."))return Get(TEXT("Errors.INVALID_LANGUAGE"));
if(Source==TEXT("Select a registered UAV video target."))return Get(TEXT("Errors.INVALID_VIDEO_TARGET"));
if(Source==TEXT("Request failed. Confirm the current selection and retry."))return Get(TEXT("Errors.Request"));
if(Source==TEXT("{0} | VIDEO TARGET: {1} | ACTIVE UAV: {2}"))return Get(TEXT("Video.Header"));
if(Source==TEXT("{0}\nModel: N/A | Link: {1}\n{2}\n{3}"))return Get(TEXT("Video.Feed"));
if(Source==TEXT("PRIMARY VIDEO / {0}"))return Get(TEXT("Video.Heading"));
if(Source==TEXT("{0} | Camera: N/A | Resolution: {1} | FPS: {2} | {3}"))return Get(TEXT("Video.Details"));
if(Source==TEXT("COMMAND CENTER | {0} | {1} | Clients {2}/3"))return Get(TEXT("Command.Header"));
if(Source==TEXT("{0} UAV | {1} ONLINE | {2} ALERTS"))return Get(TEXT("Command.Count"));
if(Source==TEXT("ALT {0} | SPD {1} | BAT {2}"))return Get(TEXT("Command.Metrics"));
if(Source==TEXT("LINK {0}\nMISSION {1}"))return Get(TEXT("Command.Link"));
if(Source==TEXT("EVENT / OPERATION LOG (UTC) | Critical {0} | Warning {1}"))return Get(TEXT("Log.Count"));
if(Source==TEXT("MAP OPERATIONS"))return Get(TEXT("Map.Header"));
if(Source==TEXT("Map initializing"))return Get(TEXT("Map.Init"));
if(Source==TEXT("Select a UAV here or on the map."))return Get(TEXT("Map.Select"));
if(Source==TEXT("LOCATE"))return Get(TEXT("Map.Locate"));
if(Source==TEXT("UAV VIDEO MONITOR CENTER"))return Get(TEXT("Video.Title"));
if(Source==TEXT("AIRCRAFT"))return Get(TEXT("Video.Aircraft"));
if(Source==TEXT("SELECT VIDEO TARGET"))return Get(TEXT("Video.SelectFeed"));
if(Source==TEXT("Model not supplied by Registry. Visual presets are not aircraft specifications."))return Get(TEXT("Video.ModelNotice"));
if(Source==TEXT("PRIMARY VIDEO"))return Get(TEXT("Video.Primary"));
if(Source==TEXT("NO TARGET\nNO ACTIVE STREAM"))return Get(TEXT("Video.NoTarget"));
if(Source==TEXT("Camera: N/A"))return Get(TEXT("Video.Camera"));
if(Source==TEXT("AI DETECTION"))return Get(TEXT("Video.AI"));
if(Source==TEXT("AI DETECTION NOT CONNECTED"))return Get(TEXT("Video.AINotConnected"));
if(Source==TEXT("PERSON\n\nVEHICLE\n\nUAV\n\nBEHAVIOR\n\nOTHER"))return Get(TEXT("Video.Categories"));
if(Source==TEXT("RECENT ALERTS"))return Get(TEXT("Video.RecentAlerts"));
if(Source==TEXT("NO ACTIVE ALERTS"))return Get(TEXT("Video.NoAlerts"));
if(Source==TEXT("Reserved fields\nTitle · UAV · Target\nTimestamp · Severity"))return Get(TEXT("Video.ReservedFields"));
if(Source==TEXT("UAV ONLINE: N/A\nAI ENGINE: NOT CONNECTED\nSTORAGE: N/A\nNETWORK: N/A"))return Get(TEXT("Video.ReservedStatus"));
if(Source==TEXT("Reserved for future integration"))return Get(TEXT("Common.Reserved"));
if(Source==TEXT("4 VIEW"))return Get(TEXT("Video.Four"));
if(Source==TEXT("6 VIEW"))return Get(TEXT("Video.Six"));
if(Source==TEXT("FOCUS"))return Get(TEXT("Video.Focus"));
if(Source==TEXT("SET AS ACTIVE UAV"))return Get(TEXT("Video.SetActive"));
if(Source==TEXT("RETRY"))return Get(TEXT("Video.Retry"));
if(Source==TEXT("SINGLE ACTIVE PLAYER · Secondary feeds are reserved slots · No AI backend connected"))return Get(TEXT("Video.Single"));
if(Source==TEXT("RESERVED\nNO UAV ASSIGNED"))return Get(TEXT("Video.Reserved"));
if(Source==TEXT("NO SOURCE\nAssign a video source to this UAV"))return Get(TEXT("Video.NoSource"));
if(Source==TEXT("SECURITY COMMAND CENTER"))return Get(TEXT("Command.Title"));
if(Source==TEXT("UAV FLEET"))return Get(TEXT("Command.Fleet"));
if(Source==TEXT("Waiting for Registry"))return Get(TEXT("Command.Waiting"));
if(Source==TEXT("ALERTS · Last 200 / Session"))return Get(TEXT("Command.Alerts"));
if(Source==TEXT("TACTICAL DATA"))return Get(TEXT("Command.Tactical"));
if(Source==TEXT("No UAV selected"))return Get(TEXT("Command.NoSelection"));
if(Source==TEXT("ALT N/A | SPD N/A | BAT N/A"))return Get(TEXT("Command.NoMetrics"));
if(Source==TEXT("LINK N/A | MISSION N/A"))return Get(TEXT("Command.NoMission"));
if(Source==TEXT("Request results require Backend confirmation. Use Map for route editing."))return Get(TEXT("Command.ActionNotice"));
if(Source==TEXT("UAV DETAILS / ACTIONS"))return Get(TEXT("Command.Details"));
if(Source==TEXT("REQUEST PAUSE"))return Get(TEXT("Command.Pause"));
if(Source==TEXT("REQUEST RESUME"))return Get(TEXT("Command.Resume"));
if(Source==TEXT("No alerts in this session"))return Get(TEXT("Command.NoAlerts"));
if(Source==TEXT("SELECT UAV"))return Get(TEXT("Command.Select"));
if(Source==TEXT("MARK HANDLED"))return Get(TEXT("Command.Handled"));
if(Source==TEXT("REMOVE RECORD"))return Get(TEXT("Command.Remove"));
if(Source==TEXT("CLEAR DISPLAY"))return Get(TEXT("Log.Clear"));
if(Source==TEXT("No events in this view"))return Get(TEXT("Log.Empty"));
if(Source==TEXT("ONLINE"))return Get(TEXT("Common.Online"));
if(Source==TEXT("LOST"))return Get(TEXT("Common.Lost"));
if(Source==TEXT("OFFLINE"))return Get(TEXT("Common.Offline"));
if(Source==TEXT("SELECTED"))return Get(TEXT("Common.Selected"));
if(Source==TEXT("SELECT"))return Get(TEXT("Common.Select"));
if(Source==TEXT("EXPAND"))return Get(TEXT("Common.Expand"));
if(Source==TEXT("COLLAPSE"))return Get(TEXT("Common.Collapse"));
if(Source==TEXT("RENAME"))return Get(TEXT("Common.Rename"));
if(Source==TEXT("RESERVED"))return Get(TEXT("Status.RESERVED"));
if(Source==TEXT("INFO"))return Get(TEXT("Status.INFO"));
if(Source==TEXT("WARNING"))return Get(TEXT("Status.WARNING"));
if(Source==TEXT("CRITICAL"))return Get(TEXT("Status.CRITICAL"));
if(Source==TEXT("RECORD"))return Get(TEXT("Status.RECORD"));
if(Source==TEXT("PLAYBACK"))return Get(TEXT("Status.PLAYBACK"));
if(Source==TEXT("SCREENSHOT"))return Get(TEXT("Status.SCREENSHOT"));
if(Source==TEXT("ALERT FILTER"))return Get(TEXT("Status.ALERT_FILTER"));
if(Source==TEXT("NO TARGET"))return Get(TEXT("Status.NO_TARGET"));
if(Source==TEXT("NO SOURCE"))return Get(TEXT("Status.NO_SOURCE"));
if(Source==TEXT("CONNECTING"))return Get(TEXT("Status.CONNECTING"));
if(Source==TEXT("PAGE READY / WAITING VIDEO"))return Get(TEXT("Status.PAGE_READY___WAITING_VIDEO"));
if(Source==TEXT("PLAYING"))return Get(TEXT("Status.PLAYING"));
if(Source==TEXT("STALLED / NO FRAMES"))return Get(TEXT("Status.STALLED___NO_FRAMES"));
if(Source==TEXT("OFFLINE / LOAD FAILED"))return Get(TEXT("Status.OFFLINE___LOAD_FAILED"));
if(Source==TEXT("MEDIA ERROR"))return Get(TEXT("Status.MEDIA_ERROR"));
if(Source==TEXT("RETRYING"))return Get(TEXT("Status.RETRYING"));
if(Source==TEXT("CLOSED"))return Get(TEXT("Status.CLOSED"));
if(Source==TEXT("SOURCE SET"))return Get(TEXT("Status.SOURCE_SET"));
if(Source==TEXT("VIDEO SLOT\nNO ACTIVE STREAM"))return Get(TEXT("Status.VIDEO_SLOT_NO_ACTIVE_STREAM"));
if(Source==TEXT("MONITOR"))return Get(TEXT("Status.MONITOR"));
if(Source==TEXT("PLAN_EDIT"))return Get(TEXT("Status.PLAN_EDIT"));
if(Source==TEXT("ALERT_RESPONSE"))return Get(TEXT("Status.ALERT_RESPONSE"));
if(Source==TEXT("MISSION_EXECUTION"))return Get(TEXT("Status.MISSION_EXECUTION"));
if(Source==TEXT("ALL"))return Get(TEXT("Status.ALL"));
if(Source==TEXT("SYSTEM"))return Get(TEXT("Status.SYSTEM"));
if(Source==TEXT("OPERATION"))return Get(TEXT("Status.OPERATION"));
if(Source==TEXT("ALERT"))return Get(TEXT("Status.ALERT"));
if(Source==TEXT("MISSION"))return Get(TEXT("Status.MISSION"));
if(Source==TEXT("PLAN"))return Get(TEXT("Status.PLAN"));
if(Source==TEXT("Plan {plan_name} created"))return Get(TEXT("Events.PLAN_CREATED"));
if(Source==TEXT("Plan {plan_name} updated"))return Get(TEXT("Events.PLAN_UPDATED"));
if(Source==TEXT("Plan {plan_name} copied as draft"))return Get(TEXT("Events.PLAN_COPIED"));
if(Source==TEXT("Mission {mission_name} created"))return Get(TEXT("Events.MISSION_CREATED"));
if(Source==TEXT("Mission {mission_name} renamed"))return Get(TEXT("Events.MISSION_UPDATED"));
if(Source==TEXT("Mission {target_id} deleted"))return Get(TEXT("Events.MISSION_DELETED"));
if(Source==TEXT("{uav_id} assigned to mission {mission_name}"))return Get(TEXT("Events.UAV_ASSIGNED"));
if(Source==TEXT("Mission {mission_name} UAV unassigned"))return Get(TEXT("Events.UAV_UNASSIGNED"));
if(Source==TEXT("Mission {mission_name} route saved"))return Get(TEXT("Events.ROUTE_CREATED"));
if(Source==TEXT("Mission {mission_name} route updated"))return Get(TEXT("Events.ROUTE_UPDATED"));
if(Source==TEXT("Mission {mission_name} route cleared"))return Get(TEXT("Events.ROUTE_CLEARED"));
if(Source==TEXT("Plan {plan_name} configuration checked"))return Get(TEXT("Events.PLAN_VALIDATED"));
if(Source==TEXT("Plan {plan_name} reviewed"))return Get(TEXT("Events.PLAN_REVIEWED"));
if(Source==TEXT("Plan {plan_name} deployed. Execution has not started."))return Get(TEXT("Events.PLAN_DEPLOYED"));
if(Source==TEXT("BACKEND {0} | SYNC {1} | ACTIVE {2} | v{3}"))return Get(TEXT("Common.Sync"));
if(Source==TEXT("CONNECTED"))return Get(TEXT("Common.Connected"));
if(Source==TEXT("DISCONNECTED"))return Get(TEXT("Common.Disconnected"));
if(Source==TEXT("RECONNECTING"))return Get(TEXT("Common.Reconnecting"));
if(Source==TEXT("2D TACTICAL MAP | Z{0} | {1} UAV without GPS"))return Get(TEXT("Tactical.Header"));
if(Source==TEXT("ACTIVE"))return Get(TEXT("Tactical.Active"));
if(Source==TEXT("ALERT"))return Get(TEXT("Tactical.Alert"));
if(Source==TEXT("Selected: {0} UAVs\n{1}"))return Get(TEXT("Fleet.Summary"));
if(Source==TEXT("Local rehearsal: backend refresh disabled"))return Get(TEXT("Fleet.Local"));
if(Source==TEXT("Online"))return Get(TEXT("Fleet.Online"));
if(Source==TEXT("Offline"))return Get(TEXT("Fleet.Offline"));
if(Source==TEXT("Disconnected"))return Get(TEXT("Fleet.Lost"));
if(Source==TEXT("Idle"))return Get(TEXT("Fleet.Idle"));
if(Source==TEXT("Unknown"))return Get(TEXT("Fleet.Unknown"));
if(Source==TEXT("{0} / {1} UTC {2}"))return Get(TEXT("Alarm.Severity"));
if(Source==TEXT("TARGET UAV {0} | {1}"))return Get(TEXT("Alarm.Target"));
if(Source==TEXT("Handled"))return Get(TEXT("Alarm.Handled"));
if(Source==TEXT("INFO"))return Get(TEXT("Alarm.Info"));
if(Source==TEXT("WARNING"))return Get(TEXT("Alarm.Warning"));
if(Source==TEXT("CRITICAL"))return Get(TEXT("Alarm.Critical"));
if(Source==TEXT("ALL"))return Get(TEXT("Log.All"));
if(Source==TEXT("Map coordinates are not ready. Retry when the map is loaded."))return Get(TEXT("Errors.COORDINATES_NOT_READY"));
if(Source==TEXT("SYSTEM ONLINE: N/A | {0} UAVs | {1} VIEW POSITIONS | {2} PLAYING | AI: NOT CONNECTED | {3}"))return Get(TEXT("Video.System"));
if(Source==TEXT("Route editing in progress: {0}. Review and deployment are blocked."))return Get(TEXT("Plan.EditingBlock"));
if(Source==TEXT("This client cannot perform this action."))return Get(TEXT("Errors.ROLE_FORBIDDEN"));
if(Source==TEXT("The server rejected this request. Refresh and retry."))return Get(TEXT("Errors.REQUEST_REJECTED"));
if(Source==TEXT("NO VIDEO TARGET\nChoose View Video to open a UAV feed."))return Get(TEXT("Video.NoTargetHint"));
if(Source==TEXT("PRIMARY"))return Get(TEXT("Fleet.Primary"));
if(Source==TEXT("MULTI SELECTED"))return Get(TEXT("Fleet.Multi"));
if(Source==TEXT("{0}\nConfigured visual: {1}\nAircraft model is not supplied by Descriptor."))return Get(TEXT("Video.DescriptorTip"));
if(Source==TEXT("Backend connected"))return Get(TEXT("Events.backend_connected"));
if(Source==TEXT("Backend reconnected"))return Get(TEXT("Events.backend_reconnected"));
if(Source==TEXT("Backend disconnected"))return Get(TEXT("Events.backend_disconnected"));
if(Source==TEXT("{source} client connected"))return Get(TEXT("Events.client_connected"));
if(Source==TEXT("{source} client disconnected"))return Get(TEXT("Events.client_disconnected"));
if(Source==TEXT("Active UAV changed: {target}"))return Get(TEXT("Events.active_uav_changed"));
if(Source==TEXT("Alert selected: {target}"))return Get(TEXT("Events.alert_selected"));
if(Source==TEXT("Operation mode changed: {target}"))return Get(TEXT("Events.operation_mode_changed"));
if(Source==TEXT("Entered alert response"))return Get(TEXT("Events.enter_alert_response"));
if(Source==TEXT("Mission selected: {target}"))return Get(TEXT("Events.active_mission_id_changed"));
if(Source==TEXT("Plan selected: {target}"))return Get(TEXT("Events.active_security_plan_id_changed"));
if(Source==TEXT("Area selected: {target}"))return Get(TEXT("Events.active_area_id_changed"));
if(Source==TEXT("DRONE SECURITY COMMAND SYSTEM"))return Get(TEXT("Stage1.Name"));
if(Source==TEXT("COMMAND CENTER"))return Get(TEXT("Stage1.Command"));
if(Source==TEXT("TACTICAL MAP"))return Get(TEXT("Stage1.Map"));
if(Source==TEXT("VIDEO INTELLIGENCE"))return Get(TEXT("Stage1.Video"));
if(Source==TEXT("{0}  |  {1}"))return Get(TEXT("Stage1.Identity"));
if(Source==TEXT("{0}   |   CMD {1}   MAP {2}   VID {3}"))return Get(TEXT("Stage1.Health"));
if(Source==TEXT("SYSTEM READY"))return Get(TEXT("Stage1.System.READY"));
if(Source==TEXT("SYSTEM DEGRADED"))return Get(TEXT("Stage1.System.DEGRADED"));
if(Source==TEXT("BACKEND OFFLINE / RECONNECTING"))return Get(TEXT("Stage1.System.OFFLINE"));
if(Source==TEXT("ONLINE"))return Get(TEXT("Stage1.Online"));
if(Source==TEXT("OFFLINE"))return Get(TEXT("Stage1.Offline"));
if(Source==TEXT("Language {0}  |  {1}"))return Get(TEXT("Stage1.Detail"));
if(Source==TEXT("SINGLE-SCREEN MULTI-WINDOW  |  Focus a window for detailed work"))return Get(TEXT("Launcher.Layout"));
if(Source==TEXT("Launch System"))return Get(TEXT("Launcher.Start"));
if(Source==TEXT("Shutdown System"))return Get(TEXT("Launcher.Shutdown"));
if(Source==TEXT("Restart"))return Get(TEXT("Launcher.Restart"));
if(Source==TEXT("Stop"))return Get(TEXT("Launcher.Stop"));
if(Source==TEXT("Focus"))return Get(TEXT("Launcher.Focus"));
if(Source==TEXT("Arrange Windows"))return Get(TEXT("Launcher.Arrange"));
if(Source==TEXT("CONNECTED TO EXISTING INSTANCE"))return Get(TEXT("Launcher.Existing"));
if(Source==TEXT("Operation failed:"))return Get(TEXT("Launcher.Error"));
if(Source==TEXT("STARTING"))return Get(TEXT("Launcher.STARTING"));
if(Source==TEXT("SYSTEM READY"))return Get(TEXT("Launcher.READY"));
if(Source==TEXT("SYSTEM DEGRADED"))return Get(TEXT("Launcher.DEGRADED"));
if(Source==TEXT("FAILED"))return Get(TEXT("Launcher.FAILED"));
if(Source==TEXT("STOPPING"))return Get(TEXT("Launcher.STOPPING"));
if(Source==TEXT("STOPPED"))return Get(TEXT("Launcher.STOPPED"));
if(Source==TEXT("ONLINE"))return Get(TEXT("Launcher.ONLINE"));
if(Source==TEXT("OFFLINE"))return Get(TEXT("Launcher.OFFLINE"));
if(Source==TEXT("RECONNECTING"))return Get(TEXT("Launcher.RECONNECTING"));
if(Source==TEXT("Overview"))return Get(TEXT("Nav.Overview"));
if(Source==TEXT("Security Plans"))return Get(TEXT("Nav.Plans"));
if(Source==TEXT("UAV Fleet"))return Get(TEXT("Nav.Fleet"));
if(Source==TEXT("Alerts"))return Get(TEXT("Nav.Alerts"));
if(Source==TEXT("Logs"))return Get(TEXT("Nav.Logs"));
if(Source==TEXT("+ New security plan"))return Get(TEXT("Workflow.NewPlan"));
if(Source==TEXT("Back to security plans"))return Get(TEXT("Workflow.BackPlans"));
if(Source==TEXT("Open plan"))return Get(TEXT("Workflow.OpenPlan"));
if(Source==TEXT("Create and continue"))return Get(TEXT("Workflow.CreateContinue"));
if(Source==TEXT("Next: task configuration"))return Get(TEXT("Workflow.NextTask"));
if(Source==TEXT("Basic information"))return Get(TEXT("Workflow.BasicInfo"));
if(Source==TEXT("Task configuration"))return Get(TEXT("Workflow.TaskConfig"));
if(Source==TEXT("Route editing"))return Get(TEXT("Workflow.RouteEditing"));
if(Source==TEXT("Pre-deployment check"))return Get(TEXT("Workflow.PreDeployReview"));
if(Source==TEXT("Deployed"))return Get(TEXT("Workflow.Deployed"));
if(Source==TEXT("Edit Route in Map →"))return Get(TEXT("Workflow.EditMap"));
if(Source==TEXT("Switch to Map / Retry"))return Get(TEXT("Workflow.ReturnMap"));
if(Source==TEXT("Return to task configuration"))return Get(TEXT("Workflow.BackEdit"));
if(Source==TEXT("Confirm and deploy →"))return Get(TEXT("Workflow.ConfirmDeploy"));
if(Source==TEXT("Create a new version from this plan"))return Get(TEXT("Workflow.NewVersion"));
if(Source==TEXT("Task 01"))return Get(TEXT("Workflow.DefaultTask"));
if(Source==TEXT("{0}  {1}"))return Get(TEXT("Workflow.Step"));
if(Source==TEXT("● {0}  {1}"))return Get(TEXT("Workflow.ActiveStep"));
if(Source==TEXT("{0}  ·  Tasks: {1}\nLast modified: {2}"))return Get(TEXT("Workflow.PlanCard"));
if(Source==TEXT("Editing in Map\n{0}\n{1} · {2}\nSaved waypoints: {3}"))return Get(TEXT("Workflow.MapEditingStatus"));
if(Source==TEXT("Map client offline\nYour saved draft is retained.\n{0}\n{1} · {2}\nSaved waypoints: {3}"))return Get(TEXT("Workflow.MapOfflineStatus"));
if(Source==TEXT("Save draft"))return Get(TEXT("Workflow.SaveDraft"));
if(Source==TEXT("Finish route editing"))return Get(TEXT("Workflow.FinishRoute"));
if(Source==TEXT("Continue editing"))return Get(TEXT("Workflow.ContinueEditing"));
if(Source==TEXT("Current security plan\n{0}\n{1}"))return Get(TEXT("Workflow.CurrentPlan"));
if(Source==TEXT("Recent events"))return Get(TEXT("Workflow.RecentEvents"));
if(Source==TEXT("Create → Configure → Edit route → Check → Deploy"))return Get(TEXT("Workflow.GlobalHint"));
if(Source==TEXT("Configuration is deployed. The UAV has not started executing."))return Get(TEXT("Workflow.DeploymentNotice"));
if(Source==TEXT("Route editing started: {mission_name}"))return Get(TEXT("Events.ROUTE_EDIT_STARTED"));
if(Source==TEXT("Route draft saved: {mission_name}"))return Get(TEXT("Events.ROUTE_SAVED"));
if(Source==TEXT("Route editing completed: {mission_name}"))return Get(TEXT("Events.ROUTE_EDIT_COMPLETED"));
if(Source==TEXT("中文"))return Get(TEXT("Workflow.Chinese"));
if(Source==TEXT("English"))return Get(TEXT("Workflow.English"));
if(Source==TEXT("Undo"))return Get(TEXT("Workflow.Undo"));
if(Source==TEXT("Edit speed"))return Get(TEXT("Workflow.EditSpeed"));
if(Source==TEXT("Waypoint {0}  ·  Height {1} m  ·  Speed {2} m/s"))return Get(TEXT("Workflow.Waypoint"));
if(Source==TEXT("Distance: {0} m · Estimated: {1} min (minimum estimate speed 1 m/s)"))return Get(TEXT("Workflow.RouteStats"));
if(Source==TEXT("Plan removed; history retained: {plan_name}"))return Get(TEXT("Events.PLAN_DELETED"));
if(Source==TEXT("Confirm delete draft"))return Get(TEXT("Workflow.DeleteDraft"));
if(Source==TEXT("Duplicate"))return Get(TEXT("Workflow.Duplicate"));
if(Source==TEXT("Save and exit"))return Get(TEXT("Workflow.SaveExit"));
if(Source==TEXT("Assigned UAV"))return Get(TEXT("Workflow.AssignedUAV"));
if(Source==TEXT("Task type: Patrol"))return Get(TEXT("Workflow.Patrol"));
if(Source==TEXT("Info"))return Get(TEXT("Workflow.StepInfo"));
if(Source==TEXT("Tasks"))return Get(TEXT("Workflow.StepTask"));
if(Source==TEXT("Route"))return Get(TEXT("Workflow.StepRoute"));
if(Source==TEXT("Check"))return Get(TEXT("Workflow.StepCheck"));
if(Source==TEXT("Deploy"))return Get(TEXT("Workflow.StepDeploy"));
if(Source==TEXT("✓ Security plan deployed"))return Get(TEXT("Workflow.DeploymentSuccess"));
if(Source==TEXT("Check task assignment and saved route below."))return Get(TEXT("Workflow.CheckInformation"));
if(Source==TEXT("Backend: {0}   ·   Map: {1}"))return Get(TEXT("Workflow.Readiness"));
if(Source==TEXT("● ROUTE EDITING"))return Get(TEXT("Workflow.RouteEditBanner"));
if(Source==TEXT("Saved · editing continues"))return Get(TEXT("Map.SAVED"));
if(Source==TEXT("{0} — new version"))return Get(TEXT("Workflow.VersionName"));
if(Source==TEXT("Last saved: {0}"))return Get(TEXT("Workflow.LastSaved"));
if(Source==TEXT("Opened {plan_name}"))return Get(TEXT("Events.PLAN_OPENED"));
if(Source==TEXT("Created a new version: {plan_name}"))return Get(TEXT("Events.PLAN_VERSION_CREATED"));
if(Source==TEXT("Pre-deployment check ready: {plan_name}"))return Get(TEXT("Events.PLAN_REVIEW_READY"));
if(Source==TEXT("Task configured: {mission_name} · {uav_id}"))return Get(TEXT("Events.MISSION_CONFIGURED"));
if(Source==TEXT("Select the Map window or use Launcher Focus."))return Get(TEXT("Errors.MAP_FOCUS_MANUAL"));
if(Source==TEXT("Created: {0}"))return Get(TEXT("Workflow.CreatedAt"));
if(Source==TEXT("Route distance: {0} m"))return Get(TEXT("Workflow.ReviewDistance"));
if(Source==TEXT("Deploy Plan →"))return Get(TEXT("Execution.Deploy"));
if(Source==TEXT("Start Mission →"))return Get(TEXT("Execution.Start"));
if(Source==TEXT("Open mission"))return Get(TEXT("Execution.Open"));
if(Source==TEXT("No active mission"))return Get(TEXT("Execution.None"));
if(Source==TEXT("SIMULATION · Mock UAV"))return Get(TEXT("Execution.Simulation"));
if(Source==TEXT("Execution Monitor · SIMULATION"))return Get(TEXT("Execution.Monitor"));
if(Source==TEXT("Deployed route is read-only"))return Get(TEXT("Execution.ReadOnly"));
if(Source==TEXT("{0}\n{1} · {2}\n{3}\nWaypoint {4} / {5} · {6}"))return Get(TEXT("Execution.Summary"));
if(Source==TEXT("Confirm"))return Get(TEXT("Execution.Confirm"));
if(Source==TEXT("Pause mission"))return Get(TEXT("Execution.Pause"));
if(Source==TEXT("Resume mission"))return Get(TEXT("Execution.Resume"));
if(Source==TEXT("Return Home"))return Get(TEXT("Execution.Return"));
if(Source==TEXT("Abort mission"))return Get(TEXT("Execution.Abort"));
if(Source==TEXT("Cancel"))return Get(TEXT("Execution.Cancel"));
if(Source==TEXT("Execution history / View plan"))return Get(TEXT("Execution.History"));
if(Source==TEXT("View mission UAV video"))return Get(TEXT("Execution.Video"));
if(Source==TEXT("Execution time: {0} seconds"))return Get(TEXT("Execution.Duration"));
if(Source==TEXT("Confirm mission execution"))return Get(TEXT("Execution.StartConfirm"));
if(Source==TEXT("Simulation preflight checks deployment, route, Mock UAV availability and execution conflicts. Backend confirms every action."))return Get(TEXT("Execution.MockCheck"));
if(Source==TEXT("Confirm Return Home? Remaining waypoints will be skipped. The Mock UAV will return to its defined Home."))return Get(TEXT("Execution.returnConfirm"));
if(Source==TEXT("Confirm abort? This execution cannot be resumed. The Mock UAV will stop at its current position."))return Get(TEXT("Execution.abortConfirm"));
if(Source==TEXT("A simulated mission is active. Shutdown will persist it as paused and stop local simulation. Continue?"))return Get(TEXT("Execution.ShutdownConfirm"));
if(Source==TEXT("All waypoints completed"))return Get(TEXT("Execution.ROUTE_FINISHED"));
if(Source==TEXT("Returned to simulation Home"))return Get(TEXT("Execution.RETURNED_HOME"));
if(Source==TEXT("Mission created"))return Get(TEXT("Execution.CREATED"));
if(Source==TEXT("Checking simulation conditions"))return Get(TEXT("Execution.PREFLIGHT"));
if(Source==TEXT("Starting mission…"))return Get(TEXT("Execution.STARTING"));
if(Source==TEXT("Mission executing"))return Get(TEXT("Execution.EXECUTING"));
if(Source==TEXT("Mission paused"))return Get(TEXT("Execution.PAUSED"));
if(Source==TEXT("Returning Home"))return Get(TEXT("Execution.RETURNING"));
if(Source==TEXT("Mission completed"))return Get(TEXT("Execution.COMPLETED"));
if(Source==TEXT("Mission aborted"))return Get(TEXT("Execution.ABORTED"));
if(Source==TEXT("Mission failed"))return Get(TEXT("Execution.FAILED"));
if(Source==TEXT("Mock UAV unavailable. Check the simulation fixture and registry."))return Get(TEXT("Errors.MOCK_UAV_UNAVAILABLE"));
if(Source==TEXT("This UAV already has an active mission."))return Get(TEXT("Errors.EXECUTION_CONFLICT"));
if(Source==TEXT("Mission state changed. Review the current state and retry."))return Get(TEXT("Errors.EXECUTION_STALE"));
if(Source==TEXT("This action is unavailable in the current mission state."))return Get(TEXT("Errors.EXECUTION_STATE_INVALID"));
if(Source==TEXT("Execution no longer exists."))return Get(TEXT("Errors.EXECUTION_NOT_FOUND"));
if(Source==TEXT("The deployed route snapshot is missing."))return Get(TEXT("Errors.IMMUTABLE_ROUTE_MISSING"));
if(Source==TEXT("Execution request identifier is required."))return Get(TEXT("Errors.REQUEST_ID_REQUIRED"));
if(Source==TEXT("Request identifier conflict."))return Get(TEXT("Errors.REQUEST_ID_REUSED"));
if(Source==TEXT("Deployment snapshot is missing."))return Get(TEXT("Errors.DEPLOYMENT_MISSING"));
if(Source==TEXT("Mission execution created · {uav_id}"))return Get(TEXT("Events.EXECUTION_CREATED"));
if(Source==TEXT("Simulation preflight started · {uav_id}"))return Get(TEXT("Events.PREFLIGHT_STARTED"));
if(Source==TEXT("Simulation preflight passed · {uav_id}"))return Get(TEXT("Events.PREFLIGHT_PASSED"));
if(Source==TEXT("Mission starting · {uav_id}"))return Get(TEXT("Events.MISSION_STARTING"));
if(Source==TEXT("Mission started · {uav_id}"))return Get(TEXT("Events.MISSION_STARTED"));
if(Source==TEXT("Mission paused · {uav_id}"))return Get(TEXT("Events.MISSION_PAUSED"));
if(Source==TEXT("Mission resumed · {uav_id}"))return Get(TEXT("Events.MISSION_RESUMED"));
if(Source==TEXT("Waypoint reached · {uav_id}"))return Get(TEXT("Events.WAYPOINT_REACHED"));
if(Source==TEXT("Returning Home · {uav_id}"))return Get(TEXT("Events.MISSION_RETURNING"));
if(Source==TEXT("Mission aborted · {uav_id}"))return Get(TEXT("Events.MISSION_ABORTED"));
if(Source==TEXT("Mock aircraft returned to the startup formation"))return Get(TEXT("Events.MOCK_STARTUP_RESET"));
if(Source==TEXT("Ended when the Mock session restarted and aircraft returned to formation."))return Get(TEXT("Errors.MOCK_STARTUP_RESET"));
if(Source==TEXT("Mission completed · {uav_id}"))return Get(TEXT("Events.MISSION_COMPLETED"));
if(Source==TEXT("Mission failed · {uav_id}"))return Get(TEXT("Events.MISSION_FAILED"));
if(Source==TEXT("Estimated distance: {0} m · Time: {1} seconds"))return Get(TEXT("Execution.Estimate"));
if(Source==TEXT("✓ Mock UAV configured"))return Get(TEXT("Execution.MockAvailable"));
if(Source==TEXT("Deployment created: {plan_name}"))return Get(TEXT("Events.DEPLOYMENT_CREATED"));
if(Source==TEXT("Confirm Start Mission"))return Get(TEXT("Execution.ConfirmStart"));
if(Source==TEXT("Confirm Return"))return Get(TEXT("Execution.ConfirmReturn"));
if(Source==TEXT("Confirm Abort"))return Get(TEXT("Execution.ConfirmAbort"));
if(Source==TEXT("No active execution conflict"))return Get(TEXT("Execution.NoConflict"));
if(Source==TEXT("Immutable deployment and saved route"))return Get(TEXT("Execution.DeploymentValid"));
if(Source==TEXT("Closed Route"))return Get(TEXT("Geometry.ClosedRoute"));
if(Source==TEXT("Move Plan"))return Get(TEXT("Geometry.MovePlan"));
if(Source==TEXT("Moving Security Plan: {0}"))return Get(TEXT("Geometry.Moving"));
if(Source==TEXT("Drag the center handle to move all routes."))return Get(TEXT("Geometry.Drag"));
if(Source==TEXT("Preview only — confirm position to save."))return Get(TEXT("Geometry.Preview"));
if(Source==TEXT("Confirm Position"))return Get(TEXT("Geometry.Confirm"));
if(Source==TEXT("Closed route requires at least 3 waypoints."))return Get(TEXT("Errors.CLOSED_ROUTE_TOO_SHORT"));
if(Source==TEXT("This plan has active or scheduled tasks. Finish or cancel them before editing, moving or deleting the plan."))return Get(TEXT("Errors.PLAN_EXECUTING"));
if(Source==TEXT("The route changed. Cancel and retry moving the plan."))return Get(TEXT("Errors.INVALID_TRANSLATION"));
if(Source==TEXT("Plan position confirmed"))return Get(TEXT("Events.PLAN_TRANSLATED"));
if(Source==TEXT("Closed route completed once"))return Get(TEXT("Events.ROUTE_CLOSURE_REACHED"));
if(Source==TEXT("Altitude (m)"))return Get(TEXT("Mission3D.Altitude"));
if(Source==TEXT("Hover time (seconds)"))return Get(TEXT("Mission3D.Hover"));
if(Source==TEXT("AGL: above ground (unchecked: ellipsoid)"))return Get(TEXT("Mission3D.AGL"));
if(Source==TEXT("APPLY WAYPOINT PARAMETERS"))return Get(TEXT("Mission3D.Apply"));
if(Source==TEXT("Selected Waypoint: P{0}"))return Get(TEXT("Mission3D.Selected"));
if(Source==TEXT("Select a waypoint to edit height and hover time"))return Get(TEXT("Mission3D.SelectPoint"));
if(Source==TEXT("Line formation spacing (m)"))return Get(TEXT("Formation.LineSpacing"));
if(Source==TEXT("SAVE ASSIGNED UAVS"))return Get(TEXT("Formation.SaveAssignment"));
if(Source==TEXT("Scheduled start (unchecked: start now)"))return Get(TEXT("Schedule.Enable"));
if(Source==TEXT("Local start date/time: YYYY-MM-DDTHH:MM:SS"))return Get(TEXT("Schedule.LocalTime"));
if(Source==TEXT("START NOW"))return Get(TEXT("Schedule.StartNow"));
if(Source==TEXT("RESCHEDULE"))return Get(TEXT("Schedule.Reschedule"));
if(Source==TEXT("CANCEL SCHEDULE"))return Get(TEXT("Schedule.Cancel"));
if(Source==TEXT("Scheduled mission starts in {0}"))return Get(TEXT("Schedule.Countdown"));
if(Source==TEXT("SCHEDULED"))return Get(TEXT("Execution.SCHEDULED"));
if(Source==TEXT("CANCELLED"))return Get(TEXT("Execution.CANCELLED"));
if(Source==TEXT("2D SITUATION MAP"))return Get(TEXT("Situation.Title"));
if(Source==TEXT("TOGGLE FOLLOW SELECTED UAV"))return Get(TEXT("Situation.Follow"));
if(Source==TEXT("Cannot resolve height. Select a waypoint, enter valid values and ensure terrain is loaded."))return Get(TEXT("Errors.ALTITUDE_DATUM_UNRESOLVED"));
if(Source==TEXT("Unknown altitude reference"))return Get(TEXT("Errors.INVALID_ALTITUDE_REFERENCE"));
if(Source==TEXT("Spacing must meet the configured minimum separation."))return Get(TEXT("Errors.INVALID_SEPARATION"));
if(Source==TEXT("Trajectory conflict: aircraft cannot maintain the required separation."))return Get(TEXT("Errors.TRAJECTORY_CONFLICT"));
if(Source==TEXT("Enter a valid future local date and time."))return Get(TEXT("Errors.INVALID_SCHEDULE"));
if(Source==TEXT("Only Line formation is currently available."))return Get(TEXT("Errors.FORMATION_UNSUPPORTED"));
if(Source==TEXT("Formation requires a nonpolar area within 10 km."))return Get(TEXT("Errors.TRAJECTORY_DOMAIN_UNSUPPORTED"));
if(Source==TEXT("P{0} | {1} m {2} | Hover {3} s"))return Get(TEXT("Mission3D.Marker"));
if(Source==TEXT("GROUP CONTROL"))return Get(TEXT("Group.Title"));
if(Source==TEXT("Selected UAVs: {0} | Preserve relative offsets | Minimum separation: {1} m"))return Get(TEXT("Group.Summary"));
if(Source==TEXT("Group anchor latitude (WGS84)"))return Get(TEXT("Group.Latitude"));
if(Source==TEXT("Group anchor longitude (WGS84)"))return Get(TEXT("Group.Longitude"));
if(Source==TEXT("Group anchor ellipsoid altitude (m)"))return Get(TEXT("Group.Altitude"));
if(Source==TEXT("Available target area radius (m)"))return Get(TEXT("Group.Area"));
if(Source==TEXT("MOVE GROUP"))return Get(TEXT("Group.Move"));
if(Source==TEXT("Group command accepted by Backend"))return Get(TEXT("Group.Accepted"));
if(Source==TEXT("{0} / {1}: predicted {2} m; required {3} m. Execution blocked."))return Get(TEXT("Group.Conflict"));
if(Source==TEXT("Scheduled: {0} | Starts in {1} | {2} UAV"))return Get(TEXT("Schedule.Overview"));
if(Source==TEXT("Terrain height is unavailable or the waypoint is below ground. Load terrain and retry."))return Get(TEXT("Errors.TERRAIN_REQUIRED"));
if(Source==TEXT("{uav_id}: scheduled mission started"))return Get(TEXT("Events.SCHEDULE_STARTED"));
if(Source==TEXT("{uav_id}: start time updated"))return Get(TEXT("Events.SCHEDULE_CHANGED"));
if(Source==TEXT("{uav_id}: immediate start requested"))return Get(TEXT("Events.SCHEDULE_DUE"));
if(Source==TEXT("{uav_id}: scheduled mission cancelled"))return Get(TEXT("Events.MISSION_CANCELLED"));
if(Source==TEXT("{uav_id}: motion paused by safety separation check"))return Get(TEXT("Events.SAFETY_CONFLICT"));
if(Source==TEXT("LIVE CONTROL · Real UAV"))return Get(TEXT("Execution.Real"));
if(Source==TEXT("Execution Monitor"))return Get(TEXT("Execution.MonitorNeutral"));
if(Source==TEXT("Real adapter configured; Backend checks live readiness"))return Get(TEXT("Execution.RealAvailable"));
if(Source==TEXT("Real control requires fresh measured telemetry, calibrated coordinates and Armed/Offboard before movement. Start does not arm or take off."))return Get(TEXT("Execution.RealCheck"));
if(Source==TEXT("Confirm Return Home? Remaining waypoints will be skipped and the real UAV will be commanded to its calibrated Home."))return Get(TEXT("Execution.Real.returnConfirm"));
if(Source==TEXT("Confirm abort? The mission cannot resume. Backend requests a hold at the measured position and reports any rejection."))return Get(TEXT("Execution.Real.abortConfirm"));
if(Source==TEXT("Real UAV rejected the command. Verify Armed/Offboard and telemetry."))return Get(TEXT("Errors.REAL_COMMAND_REJECTED"));
if(Source==TEXT("Hold was not confirmed. Check the aircraft and control link immediately."))return Get(TEXT("Errors.REAL_HOLD_REJECTED"));
if(Source==TEXT("Fresh valid aircraft telemetry is unavailable."))return Get(TEXT("Errors.REAL_TELEMETRY_UNAVAILABLE"));
if(Source==TEXT("The live coordinate anchor changed. Recalibrate before control."))return Get(TEXT("Errors.REAL_ANCHOR_CHANGED_RECALIBRATE"));
if(Source==TEXT("Real mission restored paused; verify the aircraft before resuming."))return Get(TEXT("Errors.REAL_RESTART_REQUIRES_RESUME"));
if(Source==TEXT("Stored execution belongs to a different adapter. Create a new execution."))return Get(TEXT("Errors.ADAPTER_MODE_MISMATCH"));
if(Source==TEXT("Measured UAV position left the hover tolerance."))return Get(TEXT("Errors.REAL_HOVER_DRIFT"));
if(Source==TEXT("A group member failed; the entire mission group has failed and active members were asked to hold."))return Get(TEXT("Errors.GROUP_MEMBER_FAILED"));
if(Source==TEXT("Another controller owns the aircraft."))return Get(TEXT("Errors.LEGACY_CONTROLLER_BUSY"));
if(Source==TEXT("Use the security mission controls in Real mode."))return Get(TEXT("Errors.MISSION_CONTROLLER_OWNS_CONTROL"));
if(Source==TEXT("Real execution estimates are calculated from live position by Backend at start."))return Get(TEXT("Execution.RealEstimate"));
if(Source==TEXT("Real UAV or its calibrated frame is unavailable."))return Get(TEXT("Errors.REAL_UAV_UNAVAILABLE"));
if(Source==TEXT("Real adapter configuration is incomplete or invalid."))return Get(TEXT("Errors.REAL_CONFIGURATION_INVALID"));
if(Source==TEXT("Adapter position is not a valid WGS84 coordinate."))return Get(TEXT("Errors.ADAPTER_POSITION_INVALID"));
if(Source==TEXT("The motion target or speed is invalid."))return Get(TEXT("Errors.ADAPTER_COMMAND_INVALID"));
if(Source==TEXT("The real mission is outside the supported local coordinate area."))return Get(TEXT("Errors.REAL_FRAME_DOMAIN_UNSUPPORTED"));
if(Source==TEXT("Measured NED position is invalid."))return Get(TEXT("Errors.REAL_NED_INVALID"));
if(Source==TEXT("Declare the real anchor altitude datum before execution."))return Get(TEXT("Errors.REAL_ANCHOR_DATUM_UNRESOLVED"));
if(Source==TEXT("The real coordinate calibration is invalid."))return Get(TEXT("Errors.REAL_ANCHOR_INVALID"));
if(Source==TEXT("Waypoint Inspector"))return Get(TEXT("Inspector.Title"));
if(Source==TEXT("Click a waypoint to select it. Click the map to add one. Enter or leave a field to update."))return Get(TEXT("Inspector.Help"));
if(Source==TEXT("Altitude reference"))return Get(TEXT("Inspector.Reference"));
if(Source==TEXT("Speed (m/s; 0 = default)"))return Get(TEXT("Inspector.Speed"));
if(Source==TEXT("Longitude: {0}\nLatitude: {1}"))return Get(TEXT("Inspector.Coordinates"));
if(Source==TEXT("Enter finite values: hover >= 0; speed 0 to 15 m/s."))return Get(TEXT("Errors.WAYPOINT_PARAMETERS_INVALID"));
if(Source==TEXT("ROUTE EDITING | MAP VIEW LOCKED"))return Get(TEXT("Map.ViewLocked"));
if(Source==TEXT("Adjust the map view before editing."))return Get(TEXT("Map.AdjustBeforeEdit"));
if(Source==TEXT("Finish route editing before switching map mode."))return Get(TEXT("Map.FinishBeforeMode"));
if(Source==TEXT("No valid map position"))return Get(TEXT("Map.InvalidPosition"));
if(Source==TEXT("ROUTE VISUAL  |  Low → High\nAltitude {0}–{1} m ({4})\nSpeed {2}–{3} m/s · Slow ▶  Fast ▶▶▶\nDirection →  |  Conflict: red pulse"))return Get(TEXT("RouteVisual.Legend"));
if(Source==TEXT("Ellipsoid"))return Get(TEXT("RouteVisual.Ellipsoid"));
if(Source==TEXT("World height"))return Get(TEXT("RouteVisual.WorldHeight"));
if(Source==TEXT("{0} · In Use · {1}"))return Get(TEXT("Reservation.InUse"));
if(Source==TEXT("{0} is currently executing Security Plan {1} and cannot be assigned to another plan."))return Get(TEXT("Reservation.Conflict"));
if(Source==TEXT("This UAV is executing another security plan. Refresh the assignment and choose an available UAV."))return Get(TEXT("Errors.DRONE_ACTIVE_PLAN_CONFLICT"));
if(Source==TEXT("Clear unavailable selections (save to apply)"))return Get(TEXT("Reservation.ClearUnavailable"));
if(Source==TEXT("Box select UAVs"))return Get(TEXT("Fleet.BoxButton"));
if(Source==TEXT("Fleet move"))return Get(TEXT("Fleet.MoveButton"));
if(Source==TEXT("Confirm fleet move"))return Get(TEXT("Fleet.Confirm"));
if(Source==TEXT("Return to browse / Esc"))return Get(TEXT("Fleet.Browse"));
if(Source==TEXT("Drag a box. Shift adds, Ctrl toggles. Click empty space to clear."))return Get(TEXT("Fleet.BoxHint"));
if(Source==TEXT("Click a target, review the routes, then confirm. The primary UAV is the formation anchor."))return Get(TEXT("Fleet.MoveHint"));
if(Source==TEXT("Browse the map. Shift + drag also adds UAVs to the selection."))return Get(TEXT("Fleet.BrowseHint"));
if(Source==TEXT("Target: {0}, {1}\nAnchor ellipsoid height: {2} m. Relative positions and heights are preserved."))return Get(TEXT("Fleet.Target"));
if(Source==TEXT("Fleet execution: {0}"))return Get(TEXT("Fleet.Execution"));
if(Source==TEXT("Box select at least two UAVs first."))return Get(TEXT("Fleet.SelectTwo"));
if(Source==TEXT("Selection submitted; waiting for synchronization."))return Get(TEXT("Fleet.SelectionPending"));
if(Source==TEXT("Selection changed. Click a new target to review before moving."))return Get(TEXT("Fleet.SelectionChanged"));
if(Source==TEXT("Preview only. Confirm to request Backend execution; Esc cancels."))return Get(TEXT("Fleet.PreviewOnly"));
if(Source==TEXT("{0} is in use by {1}. Finish or abort that task before moving."))return Get(TEXT("Fleet.Occupied"));
if(Source==TEXT("{0} has no available position. Refresh the connection and retry."))return Get(TEXT("Fleet.TelemetryRequired"));
if(Source==TEXT("Group Move"))return Get(TEXT("Group.Owner"));
if(Source==TEXT("Selection synchronized."))return Get(TEXT("Fleet.SelectionReady"));
if(Source==TEXT("Anchor target height (ellipsoid m)"))return Get(TEXT("Fleet3D.Height"));
if(Source==TEXT("+10 m"))return Get(TEXT("Fleet3D.Up"));
if(Source==TEXT("-10 m"))return Get(TEXT("Fleet3D.Down"));
if(Source==TEXT("Current"))return Get(TEXT("Fleet3D.Current"));
if(Source==TEXT("Pick horizontal position on the current flight plane, then set height. Relative formation offsets are preserved."))return Get(TEXT("Fleet3D.HeightHint"));
if(Source==TEXT("3D fleet move: choose a position and height, review the preview, then confirm."))return Get(TEXT("Fleet3D.MoveHint"));
if(Source==TEXT("Enter a numeric height within 10,000 m of the current anchor height. No movement has been sent."))return Get(TEXT("Fleet3D.InvalidHeight"));
if(Source==TEXT("View changed. Choose a new target before confirming movement."))return Get(TEXT("Fleet3D.ViewChanged"));
if(Source==TEXT("Target anchor: {0} m"))return Get(TEXT("Fleet3D.Marker"));
if(Source==TEXT("Fleet control"))return Get(TEXT("FleetUX.Title"));
if(Source==TEXT("1  Select"))return Get(TEXT("FleetUX.Step1"));
if(Source==TEXT("2  Target"))return Get(TEXT("FleetUX.Step2"));
if(Source==TEXT("3  Review"))return Get(TEXT("FleetUX.Step3"));
if(Source==TEXT("Target height · m"))return Get(TEXT("FleetUX.Height"));
if(Source==TEXT("Next · choose destination"))return Get(TEXT("FleetUX.Next"));
if(Source==TEXT("Move again"))return Get(TEXT("FleetUX.Again"));
if(Source==TEXT("Move {0} aircraft"))return Get(TEXT("FleetUX.Confirm"));
if(Source==TEXT("Right-click a destination"))return Get(TEXT("FleetUX.NeedTarget"));
if(Source==TEXT("Choose again"))return Get(TEXT("FleetUX.Reselect"));
if(Source==TEXT("Cancel / Esc"))return Get(TEXT("FleetUX.Exit"));
if(Source==TEXT("Collapse"))return Get(TEXT("FleetUX.Hide"));
if(Source==TEXT("Coordinates"))return Get(TEXT("FleetUX.Coordinates"));
if(Source==TEXT("Hide details"))return Get(TEXT("FleetUX.HideCoordinates"));
if(Source==TEXT("Fit view"))return Get(TEXT("FleetUX.Fit"));
if(Source==TEXT("DESTINATION PREVIEW"))return Get(TEXT("FleetUX.Preview"));
if(Source==TEXT("Keep formation and height offsets."))return Get(TEXT("FleetUX.Formation"));
if(Source==TEXT("Drag to adjust, or enter exact ellipsoid height."))return Get(TEXT("FleetUX.HeightHint"));
if(Source==TEXT("Fine adjustment: ±100 m from the current anchor height."))return Get(TEXT("FleetUX.SliderHint"));
if(Source==TEXT("Current {0} m · rise {1} m"))return Get(TEXT("FleetUX.Rise"));
if(Source==TEXT("Current {0} m · descend {1} m"))return Get(TEXT("FleetUX.Descend"));
if(Source==TEXT("Current {0} m · keep height"))return Get(TEXT("FleetUX.Level"));
if(Source==TEXT("{0} aircraft · anchor {1}"))return Get(TEXT("FleetUX.Selected"));
if(Source==TEXT("No aircraft selected"))return Get(TEXT("FleetUX.EmptySelection"));
if(Source==TEXT("Left-click an aircraft or drag to select. Shift adds; Ctrl toggles."))return Get(TEXT("FleetUX.SelectHint"));
if(Source==TEXT("Selection ready. Right-click the map to preview a move."))return Get(TEXT("FleetUX.SelectionReady"));
if(Source==TEXT("Right-click a destination. Left-drag to change aircraft."))return Get(TEXT("FleetUX.PickHint"));
if(Source==TEXT("Review height and formation."))return Get(TEXT("FleetUX.ReviewHint"));
if(Source==TEXT("The group moves together. Control it here."))return Get(TEXT("FleetUX.RunningHint"));
if(Source==TEXT("Movement ended. You can choose a new target."))return Get(TEXT("FleetUX.FinishedHint"));
if(Source==TEXT("{0} · {1}"))return Get(TEXT("FleetUX.State"));
if(Source==TEXT("≈ {0} m  ·  ≈ {1} s"))return Get(TEXT("FleetUX.Travel"));
if(Source==TEXT("≈ {0} m"))return Get(TEXT("FleetUX.Distance"));
if(Source==TEXT("Latitude {0}\nLongitude {1}\nEllipsoid height {2} m"))return Get(TEXT("FleetUX.TargetCoordinates"));
if(Source==TEXT("UAV-{0}  ·  {1} m"))return Get(TEXT("FleetUX.Marker"));
if(Source==TEXT("Stop the whole group? This movement cannot be resumed."))return Get(TEXT("FleetUX.AbortHint"));
if(Source==TEXT("Keep task"))return Get(TEXT("FleetUX.Keep"));
if(Source==TEXT("Stop group"))return Get(TEXT("FleetUX.AbortConfirm"));
if(Source==TEXT("Action confirmed."))return Get(TEXT("FleetUX.Accepted"));
if(Source==TEXT("Synced · Simulation"))return Get(TEXT("FleetUX.SyncMock"));
if(Source==TEXT("Synced · Connected"))return Get(TEXT("FleetUX.SyncReady"));
if(Source==TEXT("Connecting to control service…"))return Get(TEXT("FleetUX.SyncWaiting"));
if(Source==TEXT("Preview only · not moving"))return Get(TEXT("FleetUX.PreviewOnly"));
if(Source==TEXT("Target height above surface (m)"))return Get(TEXT("FleetTerrain.AGL"));
if(Source==TEXT("Advanced: ellipsoid height"))return Get(TEXT("FleetTerrain.UseAbsolute"));
if(Source==TEXT("Use height above surface"))return Get(TEXT("FleetTerrain.UseAGL"));
if(Source==TEXT("Height above surface uses the loaded map surface as its reference; ellipsoid height is absolute. Load the surface before confirming."))return Get(TEXT("FleetTerrain.HeightHint"));
if(Source==TEXT("Surface {0} m ellipsoid · current {1} m above surface\nTarget {2} m ellipsoid"))return Get(TEXT("FleetTerrain.Datum"));
if(Source==TEXT("Selected aircraft are below the loaded surface or have less than 1 m clearance. Check the altitude datum; Mock aircraft can be raised before moving."))return Get(TEXT("FleetTerrain.SourceBelow"));
if(Source==TEXT("The selected formation intersects the loaded surface. Increase the height or choose another destination."))return Get(TEXT("FleetTerrain.TooLow"));
if(Source==TEXT("Raise Mock fleet above surface"))return Get(TEXT("FleetTerrain.Recover"));
if(Source==TEXT("Vertical Mock recovery preview only. Confirm to raise the group to at least 30 m above the surface."))return Get(TEXT("FleetTerrain.RecoveryPreview"));
if(Source==TEXT("Enter a finite height within 10 km of the anchor; surface-relative height must be at least 1 m."))return Get(TEXT("FleetTerrain.InvalidHeight"));
if(Source==TEXT("UAV-{0} · {1} m AGL"))return Get(TEXT("FleetTerrain.MarkerAGL"));
if(Source==TEXT("Target · {0} m AGL"))return Get(TEXT("FleetTerrain.TargetAGL"));
if(Source==TEXT("New security plan"))return Get(TEXT("Configuration.NewPlan"));
if(Source==TEXT("Configuring {0}. Continue configuration in Command."))return Get(TEXT("Configuration.MapContext"));
if(Source==TEXT("Finish plan configuration before starting a fleet move."))return Get(TEXT("Configuration.MoveBlocked"));
if(Source==TEXT("Active execution controls"))return Get(TEXT("ExecutionControls.Title"));
if(Source==TEXT("{0} · {1} | {2}\n{3} · {4}"))return Get(TEXT("ExecutionControls.Group"));
if(Source==TEXT("Confirm stop"))return Get(TEXT("ExecutionControls.ConfirmAbort"));
if(Source==TEXT("{0}: {1}"))return Get(TEXT("ExecutionControls.Result"));
if(Source==TEXT("Mixed member states"))return Get(TEXT("Execution.Mixed"));
if(Source==TEXT("Configuring {0} · {1} | {2}"))return Get(TEXT("Configuration.Context"));
if(Source==TEXT("Offline · local changes retained"))return Get(TEXT("Configuration.OfflineDraft"));
if(Source==TEXT("Read only · enter configuration to edit"))return Get(TEXT("Configuration.ReadOnly"));
if(Source==TEXT("Unsaved changes"))return Get(TEXT("Configuration.Unsaved"));
if(Source==TEXT("Saved"))return Get(TEXT("Configuration.Saved"));
if(Source==TEXT("Not created yet"))return Get(TEXT("Configuration.NotCreated"));
if(Source==TEXT("You have unsaved changes. Save and continue, discard these changes, or keep editing."))return Get(TEXT("Configuration.UnsavedPrompt"));
if(Source==TEXT("Save and continue"))return Get(TEXT("Configuration.SaveContinue"));
if(Source==TEXT("Discard changes and continue"))return Get(TEXT("Configuration.DiscardContinue"));
if(Source==TEXT("Keep editing"))return Get(TEXT("Configuration.KeepEditing"));
if(Source==TEXT("Continue configuration"))return Get(TEXT("Configuration.Resume"));
if(Source==TEXT("Finish configuration"))return Get(TEXT("Configuration.Finish"));
if(Source==TEXT("Task {0}"))return Get(TEXT("Configuration.NewMissionName"));
if(Source==TEXT("Return to configuration"))return Get(TEXT("Configuration.Return"));
if(Source==TEXT("Scheduled starts wait until configuration is finished."))return Get(TEXT("Configuration.ScheduledWaiting"));
if(Source==TEXT("Finish the current plan configuration first. Alerts and active task pause/stop controls remain available."))return Get(TEXT("Errors.CONFIGURATION_IN_PROGRESS"));
if(Source==TEXT("Selection changed elsewhere. Your local changes are retained; finish or discard them before switching."))return Get(TEXT("Errors.CONFIGURATION_CONTEXT_CHANGED"));
if(Source==TEXT("Editing the route for {0}. Finish the route to continue in Command."))return Get(TEXT("Configuration.MapRouteContext"));
if(Source==TEXT("Deployed plan is read only. Create a new version to make changes."))return Get(TEXT("Configuration.DeployedReadOnly"));
if(Source==TEXT("Total UAVs"))return Get(TEXT("Figma.TotalUAV"));
if(Source==TEXT("Online devices"))return Get(TEXT("Figma.OnlineDevices"));
if(Source==TEXT("Alert center"))return Get(TEXT("Figma.AlertCenter"));
if(Source==TEXT("Current task"))return Get(TEXT("Figma.CurrentTask"));
if(Source==TEXT("Current plan"))return Get(TEXT("Figma.CurrentPlan"));
if(Source==TEXT("View all ›"))return Get(TEXT("Figma.ViewAll"));
if(Source==TEXT("Close"))return Get(TEXT("Figma.Close"));
if(Source==TEXT("Refresh"))return Get(TEXT("Figma.Refresh"));
if(Source==TEXT("{0} {1}"))return Get(TEXT("Figma.RoleHealth"));
if(Source==TEXT("Finish the current configuration before opening the fleet."))return Get(TEXT("Figma.ConfigurationBusy"));
if(Source==TEXT("Device ID"))return Get(TEXT("Figma.DeviceId"));
if(Source==TEXT("Device name"))return Get(TEXT("Figma.DeviceName"));
if(Source==TEXT("Connection"))return Get(TEXT("Figma.Connection"));
if(Source==TEXT("Battery"))return Get(TEXT("Figma.Battery"));
if(Source==TEXT("Actions"))return Get(TEXT("Figma.Actions"));
if(Source==TEXT("Time (UTC)"))return Get(TEXT("Figma.TimeUTC"));
if(Source==TEXT("Alert type"))return Get(TEXT("Figma.AlertType"));
if(Source==TEXT("Type"))return Get(TEXT("Figma.EventType"));
if(Source==TEXT("Content"))return Get(TEXT("Figma.Content"));
if(Source==TEXT("View details"))return Get(TEXT("Figma.Details"));
if(Source==TEXT("No registered devices"))return Get(TEXT("Figma.NoDevices"));
if(Source==TEXT("{0} devices · Includes offline devices"))return Get(TEXT("Figma.DeviceCount"));
if(Source==TEXT("{0} alerts"))return Get(TEXT("Figma.AlertCount"));
if(Source==TEXT("{0} records"))return Get(TEXT("Figma.LogCount"));
if(Source==TEXT("Waiting for UAV location data"))return Get(TEXT("Figma.WaitingPosition"));
if(Source==TEXT("No security plans yet"))return Get(TEXT("Figma.Plans.EmptyTitle"));
if(Source==TEXT("Create a plan to configure devices and mission routes"))return Get(TEXT("Figma.Plans.EmptyHint"));
if(Source==TEXT("New security plan"))return Get(TEXT("Figma.Plans.NewPlan"));
if(Source==TEXT("Enter basic information to continue configuring your security plan"))return Get(TEXT("Figma.Plans.NewPlanHint"));
if(Source==TEXT("Enter a plan name"))return Get(TEXT("Figma.Plans.NameHint"));
if(Source==TEXT("Enter a plan description"))return Get(TEXT("Figma.Plans.DescriptionHint"));
if(Source==TEXT("Plan workflow: Create / Configure devices / Edit routes / Review / Deploy"))return Get(TEXT("Figma.Plans.Flow"));
if(Source==TEXT("UAVs"))return Get(TEXT("Figma.Fleet.Title"));
if(Source==TEXT("UAV list"))return Get(TEXT("Figma.Fleet.List"));
if(Source==TEXT("No registered UAVs"))return Get(TEXT("Figma.Fleet.Empty"));
if(Source==TEXT("Current UAV telemetry"))return Get(TEXT("Figma.Fleet.Telemetry"));
if(Source==TEXT("Altitude"))return Get(TEXT("Figma.Fleet.Altitude"));
if(Source==TEXT("Speed"))return Get(TEXT("Figma.Fleet.Speed"));
if(Source==TEXT("Battery"))return Get(TEXT("Figma.Fleet.Battery"));
if(Source==TEXT("Link"))return Get(TEXT("Figma.Fleet.Link"));
if(Source==TEXT("Task"))return Get(TEXT("Figma.Fleet.Task"));
if(Source==TEXT("UAV group control"))return Get(TEXT("Figma.Fleet.Group"));
if(Source==TEXT("Video monitor"))return Get(TEXT("Figma.Fleet.Video"));
if(Source==TEXT("No video signal"))return Get(TEXT("Figma.Fleet.NoVideo"));
if(Source==TEXT("Choose a UAV, then open it in the Video client"))return Get(TEXT("Figma.Fleet.VideoHint"));
if(Source==TEXT("Video client target: {0}"))return Get(TEXT("Figma.Fleet.VideoTarget"));
if(Source==TEXT("Add to group"))return Get(TEXT("Figma.Fleet.Join"));
if(Source==TEXT("Set primary"))return Get(TEXT("Figma.Fleet.SetPrimary"));
if(Source==TEXT("Edit label"))return Get(TEXT("Figma.Fleet.Label"));
if(Source==TEXT("Battery {0}  ·  Task {1}"))return Get(TEXT("Figma.Fleet.RowDetail"));
if(Source==TEXT("No UAV selected"))return Get(TEXT("Figma.Fleet.NoSelection"));
if(Source==TEXT("Edit UAV label"))return Get(TEXT("Figma.Fleet.LabelTitle"));
if(Source==TEXT("Display name"))return Get(TEXT("Figma.Fleet.LabelName"));
if(Source==TEXT("Label color"))return Get(TEXT("Figma.Fleet.LabelColor"));
if(Source==TEXT("Font size"))return Get(TEXT("Figma.Fleet.LabelSize"));
if(Source==TEXT("Only affects the UAV label"))return Get(TEXT("Figma.Fleet.LabelScope"));
if(Source==TEXT("Label preview"))return Get(TEXT("Figma.Fleet.LabelPreview"));
if(Source==TEXT("Confirm"))return Get(TEXT("Figma.Fleet.Confirm"));
if(Source==TEXT("Assembling"))return Get(TEXT("Figma.Fleet.Task.Assembling"));
if(Source==TEXT("Moving"))return Get(TEXT("Figma.Fleet.Task.Moving"));
if(Source==TEXT("Scouting"))return Get(TEXT("Figma.Fleet.Task.Scouting"));
if(Source==TEXT("Patrolling"))return Get(TEXT("Figma.Fleet.Task.Patrolling"));
if(Source==TEXT("Attacking"))return Get(TEXT("Figma.Fleet.Task.Attacking"));
if(Source==TEXT("Paused"))return Get(TEXT("Figma.Fleet.Task.Paused"));
if(Source==TEXT("Avoiding"))return Get(TEXT("Figma.Fleet.Task.Avoiding"));
if(Source==TEXT("Completed"))return Get(TEXT("Figma.Fleet.Task.Completed"));
if(Source==TEXT("Error"))return Get(TEXT("Figma.Fleet.Task.Error"));
if(Source==TEXT("This UAV is offline. Check its connection."))return Get(TEXT("Figma.Fleet.VideoOffline"));
if(Source==TEXT("View in Video client"))return Get(TEXT("Figma.Fleet.VideoClient"));
if(Source==TEXT("Current target: {0}. Use the upper-right button to view it in the Video client."))return Get(TEXT("Figma.Fleet.VideoTargetHint"));
if(Source==TEXT("Selection changed in another operation. Please retry."))return Get(TEXT("Figma.Fleet.SelectionSuperseded"));
if(Source==TEXT("Active tasks ({0})"))return Get(TEXT("Overview.ActiveTasks"));
if(Source==TEXT("View task"))return Get(TEXT("Overview.ViewTask"));
if(Source==TEXT("Free movement"))return Get(TEXT("Overview.FreeMove"));
if(Source==TEXT("{0} · {1} · {2}"))return Get(TEXT("Overview.TaskState"));
if(Source==TEXT("{0} · {1}"))return Get(TEXT("Overview.TaskMembers"));
if(Source==TEXT("Tasks in this plan"))return Get(TEXT("Plan.Tasks.Title"));
if(Source==TEXT("Choose a task to edit or execute. Checking and deploying a plan does not start a task."))return Get(TEXT("Plan.Tasks.Hint"));
if(Source==TEXT("UAVs: {0} · Waypoints: {1}"))return Get(TEXT("Plan.Tasks.Assignment"));
if(Source==TEXT("Configuration: {0} · Execution: {1}"))return Get(TEXT("Plan.Tasks.State"));
if(Source==TEXT("Select task"))return Get(TEXT("Plan.Tasks.Select"));
if(Source==TEXT("Selected"))return Get(TEXT("Plan.Tasks.Selected"));
if(Source==TEXT("Edit task"))return Get(TEXT("Plan.Tasks.Edit"));
if(Source==TEXT("Edit in new version"))return Get(TEXT("Plan.Tasks.EditVersion"));
if(Source==TEXT("Execute task"))return Get(TEXT("Plan.Tasks.Execute"));
if(Source==TEXT("Check and deploy"))return Get(TEXT("Plan.Tasks.CheckDeploy"));
if(Source==TEXT("View execution"))return Get(TEXT("Plan.Tasks.ViewExecution"));
if(Source==TEXT("Map movement"))return Get(TEXT("MapMove.Title"));
if(Source==TEXT("Select at least one aircraft to move."))return Get(TEXT("MapMove.SelectAircraft"));
if(Source==TEXT("Select an aircraft icon. Shift adds aircraft; Ctrl toggles selection."))return Get(TEXT("MapMove.SelectHint"));
if(Source==TEXT("{0} selected · Click a map destination to preview. Drag to pan; wheel to zoom."))return Get(TEXT("MapMove.ClickHint"));
if(Source==TEXT("Move {0} aircraft to {1}, {2} · altitude {3} m. Confirm to send; click the map to choose again."))return Get(TEXT("MapMove.Preview"));
if(Source==TEXT("Confirm movement"))return Get(TEXT("MapMove.Confirm"));
if(Source==TEXT("Edit view: middle drag to pan, right drag to orbit in 3D, wheel to zoom. Left mouse edits waypoints."))return Get(TEXT("MapMove.EditCameraHint"));
if(Source==TEXT("{0} · Mock"))return Get(TEXT("MapMove.MockMarker"));
if(Source==TEXT("Backend online"))return Get(TEXT("Figma.Header.BackendOnline"));
if(Source==TEXT("Backend offline"))return Get(TEXT("Figma.Header.BackendOffline"));
if(Source==TEXT("Synchronized"))return Get(TEXT("Figma.Header.Synced"));
if(Source==TEXT("Sync pending"))return Get(TEXT("Figma.Header.SyncPending"));
if(Source==TEXT("Video intelligence"))return Get(TEXT("VideoUI.Title"));
if(Source==TEXT("One active player · Other slots reserved · AI not connected"))return Get(TEXT("VideoUI.Single"));
if(Source==TEXT("Aircraft"))return Get(TEXT("VideoUI.Aircraft"));
if(Source==TEXT("Select a video target"))return Get(TEXT("VideoUI.SelectSource"));
if(Source==TEXT("Visual presets do not represent aircraft specifications."))return Get(TEXT("VideoUI.ModelDisclaimer"));
if(Source==TEXT("Primary video"))return Get(TEXT("VideoUI.Primary"));
if(Source==TEXT("Video target"))return Get(TEXT("VideoUI.Current"));
if(Source==TEXT("AI detection"))return Get(TEXT("VideoUI.Detection"));
if(Source==TEXT("People"))return Get(TEXT("VideoUI.People"));
if(Source==TEXT("Vehicles"))return Get(TEXT("VideoUI.Vehicles"));
if(Source==TEXT("UAVs"))return Get(TEXT("VideoUI.Drones"));
if(Source==TEXT("Abnormal behavior"))return Get(TEXT("VideoUI.Behavior"));
if(Source==TEXT("Other targets"))return Get(TEXT("VideoUI.Other"));
if(Source==TEXT("Recent alerts"))return Get(TEXT("VideoUI.Alerts"));
if(Source==TEXT("No active alerts"))return Get(TEXT("VideoUI.NoAlerts"));
if(Source==TEXT("Received alerts · This session"))return Get(TEXT("VideoUI.AlertSession"));
if(Source==TEXT("System status"))return Get(TEXT("VideoUI.System"));
if(Source==TEXT("Online aircraft"))return Get(TEXT("VideoUI.Online"));
if(Source==TEXT("AI engine"))return Get(TEXT("VideoUI.AI"));
if(Source==TEXT("Storage"))return Get(TEXT("VideoUI.Storage"));
if(Source==TEXT("Backend"))return Get(TEXT("VideoUI.Network"));
if(Source==TEXT("Not connected"))return Get(TEXT("VideoUI.NotConnected"));
if(Source==TEXT("Unavailable"))return Get(TEXT("VideoUI.Unavailable"));
if(Source==TEXT("4 slots"))return Get(TEXT("VideoUI.Four"));
if(Source==TEXT("6 slots"))return Get(TEXT("VideoUI.Six"));
if(Source==TEXT("Focus"))return Get(TEXT("VideoUI.Focus"));
if(Source==TEXT("Select this UAV"))return Get(TEXT("VideoUI.SetActive"));
if(Source==TEXT("Explicitly select this UAV in the shared context. This action does not change the video target."))return Get(TEXT("VideoUI.SetActiveHint"));
if(Source==TEXT("Selected: {0}"))return Get(TEXT("VideoUI.ActiveSelection"));
if(Source==TEXT("Retry"))return Get(TEXT("VideoUI.Retry"));
if(Source==TEXT("Record"))return Get(TEXT("VideoUI.Record"));
if(Source==TEXT("Playback"))return Get(TEXT("VideoUI.Playback"));
if(Source==TEXT("Capture"))return Get(TEXT("VideoUI.Screenshot"));
if(Source==TEXT("Filter"))return Get(TEXT("VideoUI.Filter"));
if(Source==TEXT("This capability is not connected yet."))return Get(TEXT("VideoUI.Unsupported"));
if(Source==TEXT("Reserved slot"))return Get(TEXT("VideoUI.Reserved"));
if(Source==TEXT("No UAV assigned"))return Get(TEXT("VideoUI.Unassigned"));
if(Source==TEXT("Model: unavailable"))return Get(TEXT("VideoUI.ModelUnavailable"));
if(Source==TEXT("No video source"))return Get(TEXT("VideoUI.NoSource"));
if(Source==TEXT("Click to view"))return Get(TEXT("VideoUI.InactiveSource"));
if(Source==TEXT("Source configured"))return Get(TEXT("VideoUI.SourceSet"));
if(Source==TEXT("No video target"))return Get(TEXT("VideoUI.NoTarget"));
if(Source==TEXT("Configure a video source for this UAV"))return Get(TEXT("VideoUI.ConfigureSource"));
if(Source==TEXT("Waiting for decoded video frames"))return Get(TEXT("VideoUI.WaitingFrames"));
if(Source==TEXT("Reconnect the current video source"))return Get(TEXT("VideoUI.RetryHint"));
if(Source==TEXT("Select a UAV with a configured video source to retry."))return Get(TEXT("VideoUI.RetryUnavailable"));
if(Source==TEXT("N/A"))return Get(TEXT("VideoUI.NA"));
if(Source==TEXT("Map operations"))return Get(TEXT("MapUI.Operations"));
if(Source==TEXT("Mission route"))return Get(TEXT("MapUI.MissionRoute"));
if(Source==TEXT("View video"))return Get(TEXT("MapUI.ViewVideo"));
if(Source==TEXT("Current device"))return Get(TEXT("MapUI.CurrentDevice"));
if(Source==TEXT("Select a device"))return Get(TEXT("MapUI.SelectDevice"));
if(Source==TEXT("Middle: pan · Right: orbit · Wheel: zoom"))return Get(TEXT("MapUI.CameraHint"));
if(Source==TEXT("Left: select · Right-click: target · Middle: pan"))return Get(TEXT("MapUI.FleetPointerHint"));
if(Source==TEXT("Left-click or drag: select aircraft. Right-click: preview destination, then confirm. Middle-drag: pan. Right-drag: pan in 2D, orbit in 3D. Wheel: zoom."))return Get(TEXT("MapUI.FleetPointerHelp"));
if(Source==TEXT("Route editor"))return Get(TEXT("MapUI.RouteEditor"));
if(Source==TEXT("Default flight speed"))return Get(TEXT("MapUI.DefaultSpeed"));
if(Source==TEXT("Used for newly added waypoints."))return Get(TEXT("MapUI.DefaultSpeedHint"));
if(Source==TEXT("m/s · 0 = unlimited, or 0.5–15"))return Get(TEXT("MapUI.SpeedRange"));
if(Source==TEXT("Enter 0 (unlimited) or a speed from 0.5 to 15 m/s."))return Get(TEXT("MapUI.SpeedInvalid"));
if(Source==TEXT("Copy plan"))return Get(TEXT("Plan.CopyExplicit"));
if(Source==TEXT("Edit plan"))return Get(TEXT("Plan.EditExisting"));
if(Source==TEXT("This plan is deployed. Edit this plan to save changes under the same ID, then validate and deploy again. Previous deployments remain in history."))return Get(TEXT("Plan.EditDeployedHint"));
if(Source==TEXT("Delete plan"))return Get(TEXT("Plan.Delete.Action"));
if(Source==TEXT("Confirm deletion"))return Get(TEXT("Plan.Delete.Confirm"));
if(Source==TEXT("Review latest version"))return Get(TEXT("Plan.Delete.Refresh"));
if(Source==TEXT("Enter the exact plan name"))return Get(TEXT("Plan.Delete.NameHint"));
if(Source==TEXT("Delete “{0}” from the plan list? Enter the exact name below to confirm. Current tasks and routes will be archived; deployment and execution history will be retained. Active or scheduled tasks must be finished or cancelled first."))return Get(TEXT("Plan.Delete.Prompt"));
if(Source==TEXT("The confirmation name does not match this plan. Review the latest version and enter its exact name again."))return Get(TEXT("Errors.DELETE_CONFIRMATION_REQUIRED"));
if(Source==TEXT("The plan changed after you opened this action. Review the latest version and confirm again."))return Get(TEXT("Errors.CONTENT_REVISION_CONFLICT"));
if(Source==TEXT("Open this plan for configuration before editing it."))return Get(TEXT("Errors.CONFIGURATION_REQUIRED"));
if(Source==TEXT("This plan no longer exists. Close this dialog and return to the plan list."))return Get(TEXT("Errors.PLAN_NOT_FOUND"));
if(Source==TEXT("This plan's deployment state changed. Reload the plan and retry."))return Get(TEXT("Errors.PLAN_NOT_DEPLOYED"));
if(Source==TEXT("Plan {plan_name} reopened for editing"))return Get(TEXT("Events.PLAN_REOPENED"));
if(Source==TEXT("Execution controls"))return Get(TEXT("FleetUX.ExecutionControls"));
if(Source==TEXT("MAP OPERATIONS"))return Get(TEXT("Map.Header"));
if(Source==TEXT("Map initializing"))return Get(TEXT("Map.Init"));
if(Source==TEXT("Select a UAV here or on the map."))return Get(TEXT("Map.Select"));
if(Source==TEXT("Locate"))return Get(TEXT("Map.Locate"));
if(Source==TEXT("无人机视频监看系统  /  UAV VIDEO MONITOR CENTER"))return Get(TEXT("Video.Title"));
if(Source==TEXT("AIRCRAFT\n飞机列表"))return Get(TEXT("Video.Aircraft"));
if(Source==TEXT("SELECT PRIMARY FEED"))return Get(TEXT("Video.SelectFeed"));
if(Source==TEXT("MODEL N/A = no aircraft model field\nVisual presets are not aircraft specifications."))return Get(TEXT("Video.ModelNotice"));
if(Source==TEXT("PRIMARY VIDEO"))return Get(TEXT("Video.Primary"));
if(Source==TEXT("NO UAV SELECTED\nNO ACTIVE STREAM"))return Get(TEXT("Video.NoTarget"));
if(Source==TEXT("NO UAV REGISTERED\nNO ACTIVE STREAM"))return Get(TEXT("Video.NoTarget"));
if(Source==TEXT("Camera: N/A"))return Get(TEXT("Video.Camera"));
if(Source==TEXT("AI DETECTION\n智能检测"))return Get(TEXT("Video.AI"));
if(Source==TEXT("AI DETECTION NOT CONNECTED"))return Get(TEXT("Video.AINotConnected"));
if(Source==TEXT("人员  /  PERSON\n\n车辆  /  VEHICLE\n\n无人机  /  UAV\n\n异常行为  /  BEHAVIOR\n\n其他目标  /  OTHER"))return Get(TEXT("Video.Categories"));
if(Source==TEXT("RECENT ALERTS\n最近告警"))return Get(TEXT("Video.RecentAlerts"));
if(Source==TEXT("NO ACTIVE ALERTS"))return Get(TEXT("Video.NoAlerts"));
if(Source==TEXT("Reserved fields\nTitle · UAV · Target\nTimestamp · Severity"))return Get(TEXT("Video.ReservedFields"));
if(Source==TEXT("UAV ONLINE: N/A\nAI ENGINE: NOT CONNECTED\nSTORAGE: N/A\nNETWORK: N/A"))return Get(TEXT("Video.ReservedStatus"));
if(Source==TEXT("Reserved for future integration"))return Get(TEXT("Common.Reserved"));
if(Source==TEXT("4路 / 4 VIEW"))return Get(TEXT("Video.Four"));
if(Source==TEXT("6路 / 6 VIEW"))return Get(TEXT("Video.Six"));
if(Source==TEXT("FOCUS"))return Get(TEXT("Video.Focus"));
if(Source==TEXT("SET AS ACTIVE UAV"))return Get(TEXT("Video.SetActive"));
if(Source==TEXT("REFRESH / RETRY"))return Get(TEXT("Video.Retry"));
if(Source==TEXT("SINGLE ACTIVE PLAYER  ·  Secondary feeds are reserved slots  ·  No AI backend connected"))return Get(TEXT("Video.Single"));
if(Source==TEXT("RESERVED\n\nNO UAV ASSIGNED"))return Get(TEXT("Video.Reserved"));
if(Source==TEXT("NO SOURCE\nAssign a video source to this UAV"))return Get(TEXT("Video.NoSource"));
if(Source==TEXT("COMMAND  /  安防指挥中心"))return Get(TEXT("Command.Title"));
if(Source==TEXT("UAV FLEET  /  机队态势"))return Get(TEXT("Command.Fleet"));
if(Source==TEXT("等待注册表数据"))return Get(TEXT("Command.Waiting"));
if(Source==TEXT("告警中心 · 最近200条 / 本会话"))return Get(TEXT("Command.Alerts"));
if(Source==TEXT("TACTICAL DATA  /  无人机态势"))return Get(TEXT("Command.Tactical"));
if(Source==TEXT("未选择无人机"))return Get(TEXT("Command.NoSelection"));
if(Source==TEXT("ALT N/A   SPD N/A   BAT N/A"))return Get(TEXT("Command.NoMetrics"));
if(Source==TEXT("LINK N/A  |  MISSION N/A"))return Get(TEXT("Command.NoMission"));
if(Source==TEXT("任务请求结果以返回状态为准。路径规划请使用独立 Map Client。"))return Get(TEXT("Command.ActionNotice"));
if(Source==TEXT("UAV DETAILS / ACTIONS"))return Get(TEXT("Command.Details"));
if(Source==TEXT("暂停请求"))return Get(TEXT("Command.Pause"));
if(Source==TEXT("恢复请求"))return Get(TEXT("Command.Resume"));
if(Source==TEXT("尚未收到告警（本会话）"))return Get(TEXT("Command.NoAlerts"));
if(Source==TEXT("选择无人机"))return Get(TEXT("Command.Select"));
if(Source==TEXT("标记已处理"))return Get(TEXT("Command.Handled"));
if(Source==TEXT("移除记录"))return Get(TEXT("Command.Remove"));
if(Source==TEXT("CLEAR DISPLAY"))return Get(TEXT("Log.Clear"));
if(Source==TEXT("No events in this view"))return Get(TEXT("Log.Empty"));
if(Source==TEXT("连接"))return Get(TEXT("Fleet.Online"));
if(Source==TEXT("失联"))return Get(TEXT("Fleet.Lost"));
if(Source==TEXT("离线"))return Get(TEXT("Fleet.Offline"));
if(Source==TEXT("已选择"))return Get(TEXT("Common.Selected"));
if(Source==TEXT("选择"))return Get(TEXT("Common.Select"));
if(Source==TEXT("展开"))return Get(TEXT("Common.Expand"));
if(Source==TEXT("收起"))return Get(TEXT("Common.Collapse"));
if(Source==TEXT("重命名"))return Get(TEXT("Common.Rename"));
if(Source==TEXT("在线"))return Get(TEXT("Fleet.Online"));
if(Source==TEXT("待命"))return Get(TEXT("Fleet.Idle"));
if(Source==TEXT("未知"))return Get(TEXT("Fleet.Unknown"));
if(Source==TEXT("纯本地预演：后端刷新已禁用"))return Get(TEXT("Fleet.Local"));
if(Source==TEXT("纯本地预演模式：已禁止请求后端 refresh"))return Get(TEXT("Fleet.Local"));
if(Source==TEXT("PRIMARY / 主选中"))return Get(TEXT("Fleet.Primary"));
if(Source==TEXT("MULTI / 多选中"))return Get(TEXT("Fleet.Multi"));
return FText::AsCultureInvariant(Source);
}