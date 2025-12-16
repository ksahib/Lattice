; ModuleID = './uploads/test1.cpp'
source_filename = "./uploads/test1.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%"class.std::ios_base::Init" = type { i8 }
%"class.std::mersenne_twister_engine" = type { [312 x i64], i64 }

$_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv = comdat any

@_ZStL8__ioinit = internal global %"class.std::ios_base::Init" zeroinitializer, align 1
@__dso_handle = external hidden global i8
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @_GLOBAL__sub_I_test1.cpp, ptr null }]

declare void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #0

; Function Attrs: nounwind
declare void @_ZNSt8ios_base4InitD1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #1

; Function Attrs: nofree nounwind
declare i32 @__cxa_atexit(ptr, ptr, ptr) local_unnamed_addr #2

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 {
  %1 = alloca [10 x i32], align 16
  %2 = alloca %"class.std::mersenne_twister_engine", align 8
  call void @llvm.lifetime.start.p0(i64 40, ptr nonnull %1) #9
  call void @llvm.lifetime.start.p0(i64 2504, ptr nonnull %2) #9
  store i64 1234, ptr %2, align 8, !tbaa !5
  br label %3

3:                                                ; preds = %3, %0
  %4 = phi i64 [ 1234, %0 ], [ %9, %3 ]
  %5 = phi i64 [ 1, %0 ], [ %11, %3 ]
  %6 = lshr i64 %4, 62
  %7 = xor i64 %6, %4
  %8 = mul i64 %7, 6364136223846793005
  %9 = add i64 %8, %5
  %10 = getelementptr inbounds nuw [312 x i64], ptr %2, i64 0, i64 %5
  store i64 %9, ptr %10, align 8, !tbaa !5
  %11 = add nuw nsw i64 %5, 1
  %12 = icmp eq i64 %11, 312
  br i1 %12, label %13, label %3, !llvm.loop !9

13:                                               ; preds = %3
  %14 = getelementptr inbounds nuw i8, ptr %2, i64 2496
  store i64 312, ptr %14, align 8, !tbaa !12
  %15 = tail call x86_fp80 @llvm.log.f80(x86_fp80 0xK403F8000000000000000), !tbaa !14
  %16 = tail call x86_fp80 @llvm.log.f80(x86_fp80 0xK40008000000000000000), !tbaa !14
  %17 = fdiv x86_fp80 %15, %16
  %18 = fptoui x86_fp80 %17 to i64
  %19 = add i64 %18, 52
  %20 = udiv i64 %19, %18
  %21 = tail call i64 @llvm.umax.i64(i64 %20, i64 1)
  br label %24

22:                                               ; preds = %64
  %23 = load i32, ptr %1, align 16, !tbaa !14
  call void @llvm.lifetime.end.p0(i64 2504, ptr nonnull %2) #9
  call void @llvm.lifetime.end.p0(i64 40, ptr nonnull %1) #9
  ret i32 %23

24:                                               ; preds = %13, %64
  %25 = phi i64 [ 0, %13 ], [ %74, %64 ]
  br label %29

26:                                               ; preds = %29
  %27 = fdiv double %35, %38
  %28 = fcmp ult double %27, 1.000000e+00
  br i1 %28, label %43, label %41, !prof !16

29:                                               ; preds = %29, %24
  %30 = phi i64 [ %21, %24 ], [ %39, %29 ]
  %31 = phi double [ 1.000000e+00, %24 ], [ %38, %29 ]
  %32 = phi double [ 0.000000e+00, %24 ], [ %35, %29 ]
  %33 = call noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %2)
  %34 = uitofp i64 %33 to double
  %35 = call double @llvm.fmuladd.f64(double %34, double %31, double %32)
  %36 = fpext double %31 to x86_fp80
  %37 = fmul x86_fp80 %36, 0xK403F8000000000000000
  %38 = fptrunc x86_fp80 %37 to double
  %39 = add i64 %30, -1
  %40 = icmp eq i64 %39, 0
  br i1 %40, label %26, label %29, !llvm.loop !17

41:                                               ; preds = %26
  %42 = call double @nextafter(double noundef 1.000000e+00, double noundef 0.000000e+00) #9, !tbaa !14
  br label %43

43:                                               ; preds = %26, %41
  %44 = phi double [ %42, %41 ], [ %27, %26 ]
  %45 = fadd double %44, 0.000000e+00
  br label %50

46:                                               ; preds = %50
  %47 = fptosi double %45 to i32
  %48 = fdiv double %56, %59
  %49 = fcmp ult double %48, 1.000000e+00
  br i1 %49, label %64, label %62, !prof !16

50:                                               ; preds = %50, %43
  %51 = phi i64 [ %21, %43 ], [ %60, %50 ]
  %52 = phi double [ 1.000000e+00, %43 ], [ %59, %50 ]
  %53 = phi double [ 0.000000e+00, %43 ], [ %56, %50 ]
  %54 = call noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %2)
  %55 = uitofp i64 %54 to double
  %56 = call double @llvm.fmuladd.f64(double %55, double %52, double %53)
  %57 = fpext double %52 to x86_fp80
  %58 = fmul x86_fp80 %57, 0xK403F8000000000000000
  %59 = fptrunc x86_fp80 %58 to double
  %60 = add i64 %51, -1
  %61 = icmp eq i64 %60, 0
  br i1 %61, label %46, label %50, !llvm.loop !17

62:                                               ; preds = %46
  %63 = call double @nextafter(double noundef 1.000000e+00, double noundef 0.000000e+00) #9, !tbaa !14
  br label %64

64:                                               ; preds = %46, %62
  %65 = phi double [ %63, %62 ], [ %48, %46 ]
  %66 = fadd double %65, 0.000000e+00
  %67 = fptosi double %66 to i32
  %68 = mul nsw i32 %47, %47
  %69 = mul nsw i32 %67, %67
  %70 = add nuw nsw i32 %69, %68
  %71 = icmp samesign ult i32 %70, 2
  %72 = zext i1 %71 to i32
  %73 = getelementptr inbounds nuw [10 x i32], ptr %1, i64 0, i64 %25
  store i32 %72, ptr %73, align 4, !tbaa !14
  %74 = add nuw nsw i64 %25, 1
  %75 = icmp eq i64 %74, 10
  br i1 %75, label %22, label %24, !llvm.loop !18
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare double @llvm.fmuladd.f64(double, double, double) #5

; Function Attrs: mustprogress uwtable
define linkonce_odr dso_local noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %0) local_unnamed_addr #6 comdat align 2 {
  %2 = getelementptr inbounds nuw i8, ptr %0, i64 2496
  %3 = load i64, ptr %2, align 8, !tbaa !12
  %4 = icmp ugt i64 %3, 311
  br i1 %4, label %5, label %60

5:                                                ; preds = %1, %5
  %6 = phi i64 [ %10, %5 ], [ 0, %1 ]
  %7 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %6
  %8 = load i64, ptr %7, align 8, !tbaa !5
  %9 = and i64 %8, -2147483648
  %10 = add nuw nsw i64 %6, 1
  %11 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %10
  %12 = load i64, ptr %11, align 8, !tbaa !5
  %13 = and i64 %12, 2147483646
  %14 = or disjoint i64 %13, %9
  %15 = add nuw nsw i64 %6, 156
  %16 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %15
  %17 = load i64, ptr %16, align 8, !tbaa !5
  %18 = lshr exact i64 %14, 1
  %19 = xor i64 %18, %17
  %20 = and i64 %12, 1
  %21 = icmp eq i64 %20, 0
  %22 = select i1 %21, i64 0, i64 -5403634167711393303
  %23 = xor i64 %19, %22
  store i64 %23, ptr %7, align 8, !tbaa !5
  %24 = icmp eq i64 %10, 156
  br i1 %24, label %25, label %5, !llvm.loop !19

25:                                               ; preds = %5, %25
  %26 = phi i64 [ %30, %25 ], [ 156, %5 ]
  %27 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %26
  %28 = load i64, ptr %27, align 8, !tbaa !5
  %29 = and i64 %28, -2147483648
  %30 = add nuw nsw i64 %26, 1
  %31 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %30
  %32 = load i64, ptr %31, align 8, !tbaa !5
  %33 = and i64 %32, 2147483646
  %34 = or disjoint i64 %33, %29
  %35 = add nsw i64 %26, -156
  %36 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %35
  %37 = load i64, ptr %36, align 8, !tbaa !5
  %38 = lshr exact i64 %34, 1
  %39 = xor i64 %38, %37
  %40 = and i64 %32, 1
  %41 = icmp eq i64 %40, 0
  %42 = select i1 %41, i64 0, i64 -5403634167711393303
  %43 = xor i64 %39, %42
  store i64 %43, ptr %27, align 8, !tbaa !5
  %44 = icmp eq i64 %30, 311
  br i1 %44, label %45, label %25, !llvm.loop !20

45:                                               ; preds = %25
  %46 = getelementptr inbounds nuw i8, ptr %0, i64 2488
  %47 = load i64, ptr %46, align 8, !tbaa !5
  %48 = and i64 %47, -2147483648
  %49 = load i64, ptr %0, align 8, !tbaa !5
  %50 = and i64 %49, 2147483646
  %51 = or disjoint i64 %50, %48
  %52 = getelementptr inbounds nuw i8, ptr %0, i64 1240
  %53 = load i64, ptr %52, align 8, !tbaa !5
  %54 = lshr exact i64 %51, 1
  %55 = xor i64 %54, %53
  %56 = and i64 %49, 1
  %57 = icmp eq i64 %56, 0
  %58 = select i1 %57, i64 0, i64 -5403634167711393303
  %59 = xor i64 %55, %58
  store i64 %59, ptr %46, align 8, !tbaa !5
  store i64 0, ptr %2, align 8, !tbaa !12
  br label %60

60:                                               ; preds = %45, %1
  %61 = load i64, ptr %2, align 8, !tbaa !12
  %62 = add i64 %61, 1
  store i64 %62, ptr %2, align 8, !tbaa !12
  %63 = getelementptr inbounds nuw [312 x i64], ptr %0, i64 0, i64 %61
  %64 = load i64, ptr %63, align 8, !tbaa !5
  %65 = lshr i64 %64, 29
  %66 = and i64 %65, 22906492245
  %67 = xor i64 %66, %64
  %68 = shl i64 %67, 17
  %69 = and i64 %68, 8202884508482404352
  %70 = xor i64 %69, %67
  %71 = shl i64 %70, 37
  %72 = and i64 %71, -2270628950310912
  %73 = xor i64 %72, %70
  %74 = lshr i64 %73, 43
  %75 = xor i64 %74, %73
  ret i64 %75
}

; Function Attrs: nounwind
declare double @nextafter(double noundef, double noundef) local_unnamed_addr #1

; Function Attrs: uwtable
define internal void @_GLOBAL__sub_I_test1.cpp() #7 section ".text.startup" {
  tail call void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1) @_ZStL8__ioinit)
  %1 = tail call i32 @__cxa_atexit(ptr nonnull @_ZNSt8ios_base4InitD1Ev, ptr nonnull @_ZStL8__ioinit, ptr nonnull @__dso_handle) #9
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare x86_fp80 @llvm.log.f80(x86_fp80) #8

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.umax.i64(i64, i64) #8

attributes #0 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nofree nounwind }
attributes #3 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #5 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #6 = { mustprogress uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #9 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 20.1.8 (++20250708082409+6fb913d3e2ec-1~exp1~20250708202428.132)"}
!5 = !{!6, !6, i64 0}
!6 = !{!"long", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = distinct !{!9, !10, !11}
!10 = !{!"llvm.loop.mustprogress"}
!11 = !{!"llvm.loop.unroll.disable"}
!12 = !{!13, !6, i64 2496}
!13 = !{!"_ZTSSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE", !7, i64 0, !6, i64 2496}
!14 = !{!15, !15, i64 0}
!15 = !{!"int", !7, i64 0}
!16 = !{!"branch_weights", !"expected", i32 2000, i32 1}
!17 = distinct !{!17, !10, !11}
!18 = distinct !{!18, !10, !11}
!19 = distinct !{!19, !10, !11}
!20 = distinct !{!20, !10, !11}
