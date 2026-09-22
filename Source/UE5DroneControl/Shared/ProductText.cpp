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
{TEXT("Fleet.Summary"),NSLOCTEXT("DroneOps","Fleet.Summary","UAV {0} | BAT {1}\nMISSION {2}")},
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
{TEXT("Events.PLAN_DELETED"),NSLOCTEXT("DroneOps","Events.PLAN_DELETED","Draft deleted: {plan_name}")},
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
{TEXT("Errors.PLAN_EXECUTING"),NSLOCTEXT("DroneOps","Errors.PLAN_EXECUTING","Cannot move the plan while the mission is running.")},
{TEXT("Errors.INVALID_TRANSLATION"),NSLOCTEXT("DroneOps","Errors.INVALID_TRANSLATION","The route changed. Cancel and retry moving the plan.")},
{TEXT("Events.PLAN_TRANSLATED"),NSLOCTEXT("DroneOps","Events.PLAN_TRANSLATED","Plan position confirmed")},
{TEXT("Events.ROUTE_CLOSURE_REACHED"),NSLOCTEXT("DroneOps","Events.ROUTE_CLOSURE_REACHED","Closed route completed once")},
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
if(Source==TEXT("UAV {0} | BAT {1}\nMISSION {2}"))return Get(TEXT("Fleet.Summary"));
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
if(Source==TEXT("Draft deleted: {plan_name}"))return Get(TEXT("Events.PLAN_DELETED"));
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
if(Source==TEXT("Cannot move the plan while the mission is running."))return Get(TEXT("Errors.PLAN_EXECUTING"));
if(Source==TEXT("The route changed. Cancel and retry moving the plan."))return Get(TEXT("Errors.INVALID_TRANSLATION"));
if(Source==TEXT("Plan position confirmed"))return Get(TEXT("Events.PLAN_TRANSLATED"));
if(Source==TEXT("Closed route completed once"))return Get(TEXT("Events.ROUTE_CLOSURE_REACHED"));
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