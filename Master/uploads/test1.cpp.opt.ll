; ModuleID = './uploads/test1.cpp.opt.ll'
source_filename = "./uploads/test1.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%env.struct = type { ptr, ptr }

@__const.main.a = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16
@__const.main.b = private unnamed_addr constant [5 x i32] [i32 17, i32 42, i32 23, i32 45, i32 15], align 16

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local noundef i32 @main() local_unnamed_addr #0 {
  %.loc = alloca i64, align 8
  %1 = alloca [5 x i32], align 16
  call void @llvm.lifetime.start.p0(i64 20, ptr nonnull %1) #3
  %env_raw = call ptr @malloc(i64 16)
  %env_gep = getelementptr inbounds nuw %env.struct, ptr %env_raw, i32 0, i32 0
  store ptr %1, ptr %env_gep, align 8
  %env_gep1 = getelementptr inbounds nuw %env.struct, ptr %env_raw, i32 0, i32 1
  store ptr %.loc, ptr %env_gep1, align 8
  call void @parallel_for_runtime(i64 0, i64 5, i64 1, ptr @wrapper, ptr %env_raw)
  br label %2

2:                                                ; preds = %0
  %3 = load i32, ptr %1, align 16, !tbaa !5
  call void @llvm.lifetime.end.p0(i64 20, ptr nonnull %1) #3
  ret i32 %3
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define dso_local i1 @outlined_main_loopbody(i64 %0, ptr %1, ptr %.out) #2 {
newFuncRoot:
  br label %"23"

"23":                                             ; preds = %newFuncRoot
  %2 = getelementptr inbounds nuw [5 x i32], ptr @__const.main.a, i64 0, i64 %0
  %3 = load i32, ptr %2, align 4, !tbaa !5
  %4 = getelementptr inbounds nuw [5 x i32], ptr @__const.main.b, i64 0, i64 %0
  %5 = load i32, ptr %4, align 4, !tbaa !5
  %6 = add nsw i32 %5, %3
  %7 = getelementptr inbounds nuw [5 x i32], ptr %1, i64 0, i64 %0
  store i32 %6, ptr %7, align 4, !tbaa !5
  %8 = add nuw nsw i64 %0, 1
  store i64 %8, ptr %.out, align 8
  %9 = icmp eq i64 %8, 5
  br i1 %9, label %.exitStub, label %ind_split.exitStub, !llvm.loop !9, !my.loop.parallel !12

.exitStub:                                        ; preds = %"23"
  ret i1 true

ind_split.exitStub:                               ; preds = %"23"
  ret i1 false
}

declare ptr @malloc(i64)

define void @wrapper(i64 %idx, ptr %env) {
entry:
  %fgep = getelementptr inbounds nuw %env.struct, ptr %env, i32 0, i32 0
  %fload = load ptr, ptr %fgep, align 8
  %fgep1 = getelementptr inbounds nuw %env.struct, ptr %env, i32 0, i32 1
  %fload2 = load ptr, ptr %fgep1, align 8
  %0 = call i1 @outlined_main_loopbody(i64 %idx, ptr %fload, ptr %fload2)
  ret void
}

declare void @parallel_for_runtime(i64, i64, i64, ptr, ptr)

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "outlined-loop" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 20.1.8 (++20250708082409+6fb913d3e2ec-1~exp1~20250708202428.132)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = distinct !{!9, !10, !11}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!"llvm.loop.unroll.disable"}
!12 = !{!"parallel.type=DOALL"}
