; ModuleID = 'test2.ll'
source_filename = "test2.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test() #0 {
entry:
  %cmp = icmp ne i32 1, 2
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %add = add nsw i32 2, 1
  br label %if.end

if.else:                                          ; preds = %entry
  %sub = sub nsw i32 2, 1
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %z.0 = phi i32 [ %add, %if.then ], [ %sub, %if.else ]
  br label %do.body

do.body:                                          ; preds = %do.cond, %if.end
  %b.0 = phi i32 [ 2, %if.end ], [ %sub7, %do.cond ]
  %cmp1 = icmp slt i32 %z.0, 0
  br i1 %cmp1, label %if.then2, label %if.else4

if.then2:                                         ; preds = %do.body
  %add3 = add nsw i32 %z.0, 1
  br label %if.end6

if.else4:                                         ; preds = %do.body
  %sub5 = sub nsw i32 %z.0, 1
  br label %if.end6

if.end6:                                          ; preds = %if.else4, %if.then2
  %sub7 = sub nsw i32 %b.0, 1
  br label %do.cond

do.cond:                                          ; preds = %if.end6
  %cmp8 = icmp sge i32 %sub7, 0
  br i1 %cmp8, label %do.body, label %do.end, !llvm.loop !6

do.end:                                           ; preds = %do.cond
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
