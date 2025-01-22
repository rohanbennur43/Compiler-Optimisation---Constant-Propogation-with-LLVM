; ModuleID = 'test1.ll'
source_filename = "test1.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test() #0 {
entry:
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %w.0 = phi i32 [ 5, %entry ], [ %sub, %if.end ]
  %x.0 = phi i32 [ 10, %entry ], [ %x.1, %if.end ]
  %y.0 = phi i32 [ 11, %entry ], [ %y.1, %if.end ]
  %cmp = icmp sgt i32 %w.0, 0
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %sub = sub nsw i32 %w.0, 2
  %cmp1 = icmp sgt i32 %x.0, %y.0
  br i1 %cmp1, label %if.then, label %if.else

if.then:                                          ; preds = %while.body
  %add = add nsw i32 %x.0, 1
  %sub2 = sub nsw i32 %y.0, 1
  br label %if.end

if.else:                                          ; preds = %while.body
  %sub3 = sub nsw i32 %x.0, 1
  %add4 = add nsw i32 %y.0, 1
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %x.1 = phi i32 [ %add, %if.then ], [ %sub3, %if.else ]
  %y.1 = phi i32 [ %sub2, %if.then ], [ %add4, %if.else ]
  %add5 = add nsw i32 %x.1, %y.1
  br label %while.cond, !llvm.loop !6

while.end:                                        ; preds = %while.cond
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
