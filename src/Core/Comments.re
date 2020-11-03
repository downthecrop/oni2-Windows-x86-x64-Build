open Utility;

let hasLeadingComment = (~lineComment, line) =>
  if (StringEx.isEmpty(line)) {
    false;
  } else {
    let trimmed = StringEx.trimLeft(line);
    StringEx.startsWith(~prefix=lineComment, trimmed);
  };

let%test_module "hasLeadingComment" =
  (module
   {
     let%test "empty string" = {
       hasLeadingComment(~lineComment="#", "") == false;
     };
     let%test "whitespace" = {
       hasLeadingComment(~lineComment="#", "  ") == false;
     };

     let%test "just comment" = {
       hasLeadingComment(~lineComment="#", "#") == true;
     };

     let%test "multi-character comment" = {
       hasLeadingComment(~lineComment="##", "#") == false;
     };

     let%test "multi-character comment" = {
       hasLeadingComment(~lineComment="##", "##") == true;
     };
   });

let shouldAddComment = (~lineComment, lines) => {
  !Array.exists(hasLeadingComment(~lineComment), lines);
};

let addComment = (~lineComment, line) =>
  if (hasLeadingComment(~lineComment, line)) {
    line;
  } else {
    lineComment ++ line;
  };

let removeComment = (~lineComment, line) =>
  if (hasLeadingComment(~lineComment, line)) {
    let idxToRemove =
      StringEx.findNonWhitespace(line) |> Option.value(~default=0);

    let leadingWhitespace =
      if (idxToRemove > 0) {
        String.sub(line, 0, idxToRemove);
      } else {
        "";
      };

    let len = String.length(line);
    let commentLen = String.length(lineComment);
    let afterComment =
      String.sub(
        line,
        idxToRemove + commentLen,
        len - commentLen - idxToRemove,
      );
    leadingWhitespace ++ afterComment;
  } else {
    line;
  };

let%test_module "removeComment" =
  (module
   {
     let%test "empty string" = {
       removeComment(~lineComment="#", "") == "";
     };
     let%test "whitespace" = {
       removeComment(~lineComment="#", "  ") == "  ";
     };

     let%test "just comment" = {
       removeComment(~lineComment="#", "#") == "";
     };

     let%test "just comment, leading whitespace" = {
       removeComment(~lineComment="#", "   #") == "   ";
     };

     let%test "comment with text" = {
       removeComment(~lineComment="#", "#abc") == "abc";
     };

     let%test "comment with text, leading whitespace" = {
       removeComment(~lineComment="#", "   #abc") == "   abc";
     };
   });

let toggle = (~lineComment, lines) =>
  if (shouldAddComment(~lineComment, lines)) {
    Array.map(addComment(~lineComment), lines);
  } else {
    Array.map(removeComment(~lineComment), lines);
  };

let%test_module "toggle" =
  (module
   {
     let%test "comments -> no comments" = {
       toggle(~lineComment="#", [|"#a", "#b"|]) == [|"a", "b"|];
     };
     let%test "no comments -> comments" = {
       toggle(~lineComment="#", [|"a", "b"|]) == [|"#a", "#b"|];
     };
     let%test "mixed comments -> no comments" = {
       toggle(~lineComment="#", [|"#a", "b"|]) == [|"a", "b"|];
     };
   });
