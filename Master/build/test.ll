; ModuleID = 'uploads/ffff.cpp'
source_filename = "uploads/ffff.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-redhat-linux-gnu"

@__const.main.a = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16
@__const.main.b = private unnamed_addr constant [5 x i32] [i32 17, i32 42, i32 23, i32 45, i32 15], align 16

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
  %1 = alloca [5 x i32], align 16
  call void @llvm.lifetime.start.p0(i64 20, ptr nonnull %1) #2
  br label %4

2:                                                ; preds = %4
  %3 = load i32, ptr %1, align 16, !tbaa !3
  call void @llvm.lifetime.end.p0(i64 20, ptr nonnull %1) #2
  ret i32 %3

4:                                                ; preds = %0, %4
  %5 = phi i64 [ 0, %0 ], [ %12, %4 ]
  %6 = getelementptr inbounds nuw [5 x i32], ptr @__const.main.a, i64 0, i64 %5
  %7 = load i32, ptr %6, align 4, !tbaa !3
  %8 = getelementptr inbounds nuw [5 x i32], ptr @__const.main.b, i64 0, i64 %5
  %9 = load i32, ptr %8, align 4, !tbaa !3
  %10 = add nsw i32 %9, %7
  %11 = getelementptr inbounds nuw [5 x i32], ptr %1, i64 0, i64 %5
  store i32 %10, ptr %11, align 4, !tbaa !3
  %12 = add nuw nsw i64 %5, 1
  %13 = icmp eq i64 %12, 5
  br i1 %13, label %2, label %4, !llvm.loop !7
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"uwtable", i32 2}
!2 = !{!"clang version 20.1.8 (Fedora 20.1.8-4.fc42)"}
!3 = !{!4, !4, i64 0}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C++ TBAA"}
!7 = distinct !{!7, !8, !9}
!8 = !{!"llvm.loop.mustprogress"}
!9 = !{!"llvm.loop.unroll.disable"}
