; ModuleID = 'test3.ll'
source_filename = "test3.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test() #0 {
entry:
  br label %do.body

do.body:                                          ; preds = %do.cond, %entry
  %e.0 = phi i32 [ 0, %entry ], [ %e.1, %do.cond ]
  %add = add nsw i32 1, 1
  %mul = mul nsw i32 %add, 1
  %cmp = icmp sgt i32 1, 9
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %do.body
  %mul1 = mul nsw i32 1, %mul
  %sub = sub nsw i32 %mul1, 3
  br label %if.end

if.else:                                          ; preds = %do.body
  %add2 = add nsw i32 %e.0, 1
  %div = sdiv i32 1, 2
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %e.1 = phi i32 [ %e.0, %if.then ], [ %div, %if.else ]
  br label %do.cond

do.cond:                                          ; preds = %if.end
  %cmp3 = icmp slt i32 1, 9
  br i1 %cmp3, label %do.body, label %do.end, !llvm.loop !6

do.end:                                           ; preds = %do.cond
  %add4 = add nsw i32 1, 1
  ret void
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
