#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Map/MapLabelPlacement.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapLabelPlacementTest,"DroneOps.Figma.MapLabelLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapLabelPlacementTest::RunTest(const FString&){
    for(double Width:{1380.,949.}){
        const FBox2D Bounds(FVector2D(0,216),FVector2D(Width,790));
        const TArray<FVector2D> Aircraft{{Width*.45,340},{Width*.45,366},{Width*.45,392},{Width*.45,418}};
        TArray<FBox2D> Obstacles;for(const auto& P:Aircraft)Obstacles.Add(FBox2D(P-FVector2D(20),P+FVector2D(20)));
        const FBox2D Control(FVector2D(Width-220,230),FVector2D(Width-10,530));
        Obstacles.Add(Control);
        for(const auto& P:Aircraft){FVector2D Label=FVector2D::ZeroVector;const FVector2D LabelSize(250,52);
            auto Clear=[&](const FVector2D& A,const FVector2D& S){return MapLabels::ClearOfRects(A,S,TArray<FBox2D>{Control});};
            if(!TestTrue(TEXT("Dense UAV cluster keeps a readable label in full and route-side layouts"),MapLabels::Place(P,LabelSize,Bounds,Obstacles,Clear,Label)))continue;
            const FBox2D Result(Label,Label+LabelSize);
            TestTrue(TEXT("Annotation remains inside actual map frame"),Bounds.IsInside(Label) && Bounds.IsInside(Label+LabelSize));
            for(const auto& Other:Obstacles)TestFalse(TEXT("Label avoids aircraft symbols and prior labels"),Result.Intersect(Other));
            TestFalse(TEXT("Label avoids open control card"),Result.Intersect(Control));Obstacles.Add(Result);
        }
    }
    const TArray<FBox2D> NarrowControl{FBox2D(FVector2D(1024,410),FVector2D(1396,450))};
    TestFalse(TEXT("A label enclosing a narrow control is blocked even though every corner is outside"),
        MapLabels::ClearOfRects(FVector2D(944,400),FVector2D(460,60),NarrowControl));
    TestTrue(TEXT("A separated label remains available"),MapLabels::ClearOfRects(FVector2D(944,470),FVector2D(460,60),NarrowControl));
    return true;
}
#endif
