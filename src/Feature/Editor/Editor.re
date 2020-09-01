open EditorCoreTypes;
open Oni_Core;

module GlobalState = {
  let lastId = ref(0);

  let generateId = () => {
    let id = lastId^;
    incr(lastId);
    id;
  };
};

type pixelPosition = {
  pixelX: float,
  pixelY: float,
};

type viewLine = {
  contents: BufferLine.t,
  byteOffset: int,
  characterOffset: int,
};

[@deriving show]
type t = {
  key: [@opaque] Brisk_reconciler.Key.t,
  buffer: [@opaque] EditorBuffer.t,
  editorId: EditorId.t,
  inlineElements: InlineElements.t,
  lineHeight: LineHeight.t,
  scrollX: float,
  scrollY: float,
  isScrollAnimated: bool,
  isMinimapEnabled: bool,
  minimapMaxColumnWidth: int,
  minimapScrollY: float,
  /*
   * The maximum line visible in the view.
   * TODO: This will be dependent on line-wrap settings.
   */
  maxLineLength: int,
  viewLines: int,
  cursors: [@opaque] list(BytePosition.t),
  selection: [@opaque] option(VisualRange.t),
  pixelWidth: int,
  pixelHeight: int,
};

let key = ({key, _}) => key;
let totalViewLines = ({viewLines, _}) => viewLines;
let selection = ({selection, _}) => selection;
let setSelection = (~selection, editor) => {
  ...editor,
  selection: Some(selection),
};
let clearSelection = editor => {...editor, selection: None};
let visiblePixelWidth = ({pixelWidth, _}) => pixelWidth;
let visiblePixelHeight = ({pixelHeight, _}) => pixelHeight;
let scrollY = ({scrollY, _}) => scrollY;
let scrollX = ({scrollX, _}) => scrollX;
let minimapScrollY = ({minimapScrollY, _}) => minimapScrollY;
let lineHeightInPixels = ({buffer, lineHeight, _}) =>
  lineHeight
  |> LineHeight.calculate(
       ~measuredFontHeight=EditorBuffer.font(buffer).measuredHeight,
     );

let linePaddingInPixels = ({buffer, _} as editor) =>
  (lineHeightInPixels(editor) -. EditorBuffer.font(buffer).measuredHeight)
  /. 2.;
let characterWidthInPixels = ({buffer, _}) =>
  EditorBuffer.font(buffer).spaceWidth;
let font = ({buffer, _}) => EditorBuffer.font(buffer);

let setMinimapEnabled = (~enabled, editor) => {
  ...editor,
  isMinimapEnabled: enabled,
};

let isMinimapEnabled = ({isMinimapEnabled, _}) => isMinimapEnabled;
let isScrollAnimated = ({isScrollAnimated, _}) => isScrollAnimated;

let bufferBytePositionToPixel =
    (~position: BytePosition.t, {scrollX, scrollY, buffer, _} as editor) => {
  let lineCount = EditorBuffer.numberOfLines(buffer);
  let line = position.line |> EditorCoreTypes.LineNumber.toZeroBased;
  if (line < 0 || line >= lineCount) {
    ({pixelX: 0., pixelY: 0.}, 0.);
  } else {
    let bufferLine = buffer |> EditorBuffer.line(line);

    let index = BufferLine.getIndex(~byte=position.byte, bufferLine);
    let (cursorOffset, width) =
      BufferLine.getPixelPositionAndWidth(~index, bufferLine);

    let inlineElementOffsetY =
      InlineElements.getReservedSpace(position.line, editor.inlineElements);

    let pixelX = cursorOffset -. scrollX +. 0.5;

    let pixelY =
      inlineElementOffsetY
      +. lineHeightInPixels(editor)
      *. float(line)
      -. scrollY
      +. 0.5;

    ({pixelX, pixelY}, width);
  };
};

let viewLine = (editor, lineNumber) => {
  let contents = editor.buffer |> EditorBuffer.line(lineNumber);

  {contents, byteOffset: 0, characterOffset: 0};
};

let bufferCharacterPositionToPixel =
    (~position: CharacterPosition.t, {scrollX, scrollY, buffer, _} as editor) => {
  let lineCount = EditorBuffer.numberOfLines(buffer);
  let line = position.line |> EditorCoreTypes.LineNumber.toZeroBased;
  if (line < 0 || line >= lineCount) {
    ({pixelX: 0., pixelY: 0.}, 0.);
  } else {
    let (cursorOffset, width) =
      buffer
      |> EditorBuffer.line(line)
      |> BufferLine.getPixelPositionAndWidth(~index=position.character);

    let inlineElementOffsetY =
      InlineElements.getReservedSpace(position.line, editor.inlineElements);
    let pixelX = cursorOffset -. scrollX +. 0.5;

    let pixelY =
      inlineElementOffsetY
      +. lineHeightInPixels(editor)
      *. float(line)
      -. scrollY
      +. 0.5;

    ({pixelX, pixelY}, width);
  };
};

let create = (~config, ~buffer, ()) => {
  let id = GlobalState.generateId();
  let key = Brisk_reconciler.Key.create();

  let isMinimapEnabled = EditorConfiguration.Minimap.enabled.get(config);
  let lineHeight = EditorConfiguration.lineHeight.get(config);

  {
    editorId: id,
    key,
    inlineElements: InlineElements.initial,
    lineHeight,
    isMinimapEnabled,
    isScrollAnimated: false,
    buffer,
    scrollX: 0.,
    scrollY: 0.,
    minimapMaxColumnWidth: Constants.minimapMaxColumn,
    minimapScrollY: 0.,
    viewLines: EditorBuffer.numberOfLines(buffer),
    maxLineLength: EditorBuffer.getEstimatedMaxLineLength(buffer),
    /*
     * We need an initial editor size, otherwise we'll immediately scroll the view
     * if a buffer loads prior to our first render.
     */
    cursors: [{line: EditorCoreTypes.LineNumber.zero, byte: ByteIndex.zero}],
    selection: None,
    pixelWidth: 1,
    pixelHeight: 1,
  };
};

let copy = editor => {
  let id = GlobalState.generateId();
  let key = Brisk_reconciler.Key.create();

  {...editor, key, editorId: id};
};

type scrollbarMetrics = {
  visible: bool,
  thumbSize: int,
  thumbOffset: int,
};

let getCursors = ({cursors, _}) => cursors;

let getNearestMatchingPair =
    (~characterPosition: CharacterPosition.t, ~pairs, {buffer, _}) => {
  BracketMatch.findFirst(~buffer, ~characterPosition, ~pairs)
  |> Option.map(({start, stop}: BracketMatch.pair) => (start, stop));
};

let byteToCharacter = (position: BytePosition.t, editor) => {
  let line = position.line |> EditorCoreTypes.LineNumber.toZeroBased;

  let bufferLineCount = EditorBuffer.numberOfLines(editor.buffer);

  if (line < bufferLineCount) {
    let bufferLine = EditorBuffer.line(line, editor.buffer);
    let character = BufferLine.getIndex(~byte=position.byte, bufferLine);

    Some(
      EditorCoreTypes.(CharacterPosition.{line: position.line, character}),
    );
  } else {
    None;
  };
};

let byteRangeToCharacterRange = ({start, stop}: ByteRange.t, editor) => {
  let maybeCharacterStart = byteToCharacter(start, editor);
  let maybeCharacterStop = byteToCharacter(stop, editor);

  Oni_Core.Utility.OptionEx.map2(
    (start, stop) => {CharacterRange.{start, stop}},
    maybeCharacterStart,
    maybeCharacterStop,
  );
};

let characterToByte = (position: CharacterPosition.t, editor) => {
  let line = position.line |> EditorCoreTypes.LineNumber.toZeroBased;

  let bufferLineCount = EditorBuffer.numberOfLines(editor.buffer);

  if (line < bufferLineCount) {
    let bufferLine = EditorBuffer.line(line, editor.buffer);
    let byteIndex =
      BufferLine.getByteFromIndex(~index=position.character, bufferLine);

    Some(
      EditorCoreTypes.(BytePosition.{line: position.line, byte: byteIndex}),
    );
  } else {
    None;
  };
};
let getCharacterAtPosition = (~position: CharacterPosition.t, {buffer, _}) => {
  let line = EditorCoreTypes.LineNumber.toZeroBased(position.line);
  let bufferLineCount = EditorBuffer.numberOfLines(buffer);

  if (line < bufferLineCount) {
    let bufferLine = EditorBuffer.line(line, buffer);
    try(Some(BufferLine.getUcharExn(~index=position.character, bufferLine))) {
    | _exn => None
    };
  } else {
    None;
  };
};

let getCharacterBehindCursor = ({cursors, buffer, _}) => {
  switch (cursors) {
  | [] => None
  | [cursor, ..._] =>
    let line = cursor.line |> EditorCoreTypes.LineNumber.toZeroBased;

    let bufferLineCount = EditorBuffer.numberOfLines(buffer);

    if (line < bufferLineCount) {
      let bufferLine = EditorBuffer.line(line, buffer);
      let index =
        max(
          0,
          CharacterIndex.toInt(
            BufferLine.getIndex(~byte=cursor.byte, bufferLine),
          )
          - 1,
        );
      try(
        Some(
          BufferLine.getUcharExn(
            ~index=CharacterIndex.ofInt(index),
            bufferLine,
          ),
        )
      ) {
      | _exn => None
      };
    } else {
      None;
    };
  };
};

let getCharacterUnderCursor = ({cursors, buffer, _}) => {
  switch (cursors) {
  | [] => None
  | [cursor, ..._] =>
    let line = cursor.line |> EditorCoreTypes.LineNumber.toZeroBased;

    let bufferLineCount = EditorBuffer.numberOfLines(buffer);

    if (line < bufferLineCount) {
      let bufferLine = EditorBuffer.line(line, buffer);
      let index = BufferLine.getIndex(~byte=cursor.byte, bufferLine);
      try(Some(BufferLine.getUcharExn(~index, bufferLine))) {
      | _exn => None
      };
    } else {
      None;
    };
  };
};

let getPrimaryCursor = editor => {
  let maybeCharacterCursor =
    switch (editor.cursors) {
    | [cursor, ..._] => byteToCharacter(cursor, editor)
    | [] => None
    };

  maybeCharacterCursor
  |> Option.value(
       ~default=
         CharacterPosition.{
           line: EditorCoreTypes.LineNumber.zero,
           character: CharacterIndex.zero,
         },
     );
};

let getPrimaryCursorByte = editor =>
  switch (editor.cursors) {
  | [cursor, ..._] => cursor
  | [] =>
    BytePosition.{line: EditorCoreTypes.LineNumber.zero, byte: ByteIndex.zero}
  };
let selectionOrCursorRange = editor => {
  switch (editor.selection) {
  | None =>
    let pos = getPrimaryCursorByte(editor);
    ByteRange.{
      start: BytePosition.{line: pos.line, byte: ByteIndex.zero},
      stop:
        BytePosition.{
          line: EditorCoreTypes.LineNumber.(pos.line + 1),
          byte: ByteIndex.zero,
        },
    };
  | Some(selection) => selection.range
  };
};

let setLineHeight = (~lineHeight, editor) => {...editor, lineHeight};

let getId = model => model.editorId;

let getCharacterWidth = ({buffer, _}) =>
  EditorBuffer.font(buffer).spaceWidth;

let getVisibleView = editor => {
  let {pixelHeight, _} = editor;
  int_of_float(float_of_int(pixelHeight) /. lineHeightInPixels(editor));
};

let getTotalHeightInPixels = editor =>
  int_of_float(
    float_of_int(editor.viewLines) *. lineHeightInPixels(editor),
  );

let getTotalWidthInPixels = editor =>
  int_of_float(
    float_of_int(editor.maxLineLength) *. getCharacterWidth(editor),
  );

let getVerticalScrollbarMetrics = (view, scrollBarHeight) => {
  let {pixelHeight, _} = view;
  let totalViewSizeInPixels =
    float_of_int(getTotalHeightInPixels(view) + pixelHeight);
  let thumbPercentage = float_of_int(pixelHeight) /. totalViewSizeInPixels;
  let thumbSize =
    int_of_float(thumbPercentage *. float_of_int(scrollBarHeight));

  let topF = view.scrollY /. totalViewSizeInPixels;
  let thumbOffset = int_of_float(topF *. float_of_int(scrollBarHeight));

  {thumbSize, thumbOffset, visible: true};
};

let getHorizontalScrollbarMetrics = (view, availableWidth) => {
  let availableWidthF = float_of_int(availableWidth);
  let totalViewWidthInPixels =
    float_of_int(view.maxLineLength + 1) *. getCharacterWidth(view);
  //+. availableWidthF;

  totalViewWidthInPixels <= availableWidthF
    ? {visible: false, thumbSize: 0, thumbOffset: 0}
    : {
      let thumbPercentage = availableWidthF /. totalViewWidthInPixels;
      let thumbSize = int_of_float(thumbPercentage *. availableWidthF);

      let topF = view.scrollX /. totalViewWidthInPixels;
      let thumbOffset = int_of_float(topF *. availableWidthF);

      {thumbSize, thumbOffset, visible: true};
    };
};

let getLayout = (~showLineNumbers, ~maxMinimapCharacters, view) => {
  let {pixelWidth, pixelHeight, isMinimapEnabled, _} = view;
  let layout: EditorLayout.t =
    EditorLayout.getLayout(
      ~showLineNumbers,
      ~isMinimapShown=isMinimapEnabled,
      ~maxMinimapCharacters,
      ~pixelWidth=float_of_int(pixelWidth),
      ~pixelHeight=float_of_int(pixelHeight),
      ~characterWidth=getCharacterWidth(view),
      ~characterHeight=lineHeightInPixels(view),
      ~bufferLineCount=view.viewLines,
      (),
    );

  layout;
};

let exposePrimaryCursor = editor => {
  switch (editor.cursors) {
  | [primaryCursor, ..._tail] =>
    let {bufferWidthInPixels, _}: EditorLayout.t =
      getLayout(~showLineNumbers=true, ~maxMinimapCharacters=999, editor);

    let pixelWidth = bufferWidthInPixels;

    let {pixelHeight, scrollX, scrollY, _} = editor;
    let pixelHeight = float(pixelHeight);

    let ({pixelX, pixelY}, _width) =
      bufferBytePositionToPixel(~position=primaryCursor, editor);

    let scrollOffX = getCharacterWidth(editor) *. 2.;
    let scrollOffY = lineHeightInPixels(editor);

    let availableX = pixelWidth -. scrollOffX;
    let availableY = pixelHeight -. scrollOffY;

    let adjustedScrollX =
      if (pixelX < 0.) {
        scrollX +. pixelX;
      } else if (pixelX >= availableX) {
        scrollX +. (pixelX -. availableX);
      } else {
        scrollX;
      };

    let adjustedScrollY =
      if (pixelY < 0.) {
        scrollY +. pixelY;
      } else if (pixelY >= availableY) {
        scrollY +. (pixelY -. availableY);
      } else {
        scrollY;
      };

    {...editor, scrollX: adjustedScrollX, scrollY: adjustedScrollY};

  | _ => editor
  };
};

let setCursors = (~cursors, editor) =>
  {...editor, cursors} |> exposePrimaryCursor;

let getLeftVisibleColumn = view => {
  int_of_float(view.scrollX /. getCharacterWidth(view));
};

let getLineFromPixelY = (~pixelY, editor) => {
  let lineHeight = lineHeightInPixels(editor);
  let rec loop = (accumulatedLines, accumulatedPixels, remainingInlineElements) => {

    prerr_endline(Printf.sprintf(
      "-- Accumulated Lines: %d accumulated Pixels: %f", 
      accumulatedLines, accumulatedPixels
    ));
    if (pixelY < accumulatedPixels) {
      prerr_endline ("backtrack");
      accumulatedLines - 1
    } else {
      switch (remainingInlineElements: InlineElements.t) {
      | [] => 
      prerr_endline ("All empty");
      let lineNumber = int_of_float((pixelY -. accumulatedPixels) /. lineHeight);
      prerr_endline ("linenumber: " ++ string_of_int(lineNumber));
      prerr_endline ("accumulated lines: " ++ string_of_int(accumulatedLines));
      lineNumber + accumulatedLines
      | [hd, ...tail] =>
        let additionalRegion = float(hd.line - accumulatedLines) *. lineHeight;
        if ((additionalRegion +. accumulatedPixels) >= pixelY) {
          // The line is prior to the next inline element!
          prerr_endline ("In region");
          let lineNumber = int_of_float((pixelY -. accumulatedPixels) /. lineHeight);
          lineNumber + accumulatedLines
          
        } else {
          // We need to advance
          loop(hd.line + 1, 
          accumulatedPixels +.
          additionalRegion +. hd.height +. lineHeight , tail);
        }
      }
    }
    
  };


  loop(0, 0., editor.inlineElements);
  
};

let getTopVisibleLine = editor => {
  let top = getLineFromPixelY(~pixelY=editor.scrollY, editor) + 1;
  prerr_endline ("getTopVisibleLine: " ++ string_of_int(top));
  top;
}

let getBottomVisibleLine = editor => {
  let absoluteBottomLine = getLineFromPixelY(
  ~pixelY=editor.scrollY +. float_of_int(editor.pixelHeight), editor);
  let ret = absoluteBottomLine > editor.viewLines ? editor.viewLines : absoluteBottomLine;
  prerr_endline ("getBottomVisibleLine: " ++ string_of_int(ret));
  ret;
};

let setSize = (~pixelWidth, ~pixelHeight, editor) => {
  ...editor,
  pixelWidth,
  pixelHeight,
};

let scrollToPixelY = (~pixelY as newScrollY, view) => {
  let {pixelHeight, _} = view;
  let newScrollY = max(0., newScrollY);
  let availableScroll =
    max(float_of_int(view.viewLines - 1), 0.) *. lineHeightInPixels(view);
  let newScrollY = min(newScrollY, availableScroll);

  let scrollPercentage =
    newScrollY /. (availableScroll -. float_of_int(pixelHeight));
  let minimapLineSize =
    Constants.minimapCharacterWidth + Constants.minimapCharacterHeight;
  let linesInMinimap = pixelHeight / minimapLineSize;
  let availableMinimapScroll =
    max(view.viewLines - linesInMinimap, 0) * minimapLineSize;
  let newMinimapScroll =
    scrollPercentage *. float_of_int(availableMinimapScroll);

  {
    ...view,
    isScrollAnimated: false,
    minimapScrollY: newMinimapScroll,
    scrollY: newScrollY,
  };
};

let scrollToLine = (~line, view) => {
  let pixelY = float_of_int(line) *. lineHeightInPixels(view);
  {...scrollToPixelY(~pixelY, view), isScrollAnimated: true};
};

let scrollToPixelX = (~pixelX as newScrollX, view) => {
  let newScrollX = max(0., newScrollX);

  let availableScroll =
    max(0., float_of_int(view.maxLineLength) *. getCharacterWidth(view));
  let scrollX = min(newScrollX, availableScroll);

  {...view, isScrollAnimated: false, scrollX};
};

let scrollDeltaPixelX = (~pixelX, editor) => {
  let pixelX = editor.scrollX +. pixelX;
  scrollToPixelX(~pixelX, editor);
};

let scrollToColumn = (~column, view) => {
  let pixelX = float_of_int(column) *. getCharacterWidth(view);
  {...scrollToPixelX(~pixelX, view), isScrollAnimated: true};
};

let scrollDeltaPixelY = (~pixelY, view) => {
  let pixelY = view.scrollY +. pixelY;
  scrollToPixelY(~pixelY, view);
};

let scrollToPixelXY = (~pixelX as newScrollX, ~pixelY as newScrollY, view) => {
  let {scrollX, _} = scrollToPixelX(~pixelX=newScrollX, view);
  let {scrollY, minimapScrollY, _} =
    scrollToPixelY(~pixelY=newScrollY, view);

  {...view, scrollX, scrollY, minimapScrollY};
};

let scrollDeltaPixelXY = (~pixelX, ~pixelY, view) => {
  let {scrollX, _} = scrollDeltaPixelX(~pixelX, view);
  let {scrollY, minimapScrollY, _} = scrollDeltaPixelY(~pixelY, view);

  {...view, scrollX, scrollY, minimapScrollY};
};

// PROJECTION

let project = (~line, ~column: int, ~pixelWidth: int, ~pixelHeight, editor) => {
  // TODO: Horizontal scrolling
  ignore(column);
  ignore(pixelWidth);

  let lineIdx = EditorCoreTypes.LineNumber.toZeroBased(line);
  let editorPixelY = float_of_int(lineIdx) *. lineHeightInPixels(editor);
  let totalEditorHeight = getTotalHeightInPixels(editor) |> float_of_int;
  let transformedPixelY =
    editorPixelY
    /. (totalEditorHeight +. float_of_int(editor.pixelHeight))
    *. float_of_int(pixelHeight);

  (0., transformedPixelY);
};

let projectLine = (~line, ~pixelHeight, editor) => {
  let (_x, y) =
    project(~line, ~column=0, ~pixelWidth=1, ~pixelHeight, editor);
  y;
};

let unprojectToPixel =
    (~pixelX: float, ~pixelY, ~pixelWidth: int, ~pixelHeight, editor) => {
  let totalWidth = getTotalWidthInPixels(editor) |> float_of_int;
  let x = totalWidth *. pixelX /. float_of_int(pixelWidth);

  let totalHeight = getTotalHeightInPixels(editor) |> float_of_int;
  let y = totalHeight *. pixelY /. float_of_int(pixelHeight);

  (x, y);
};

let getBufferId = ({buffer, _}) => EditorBuffer.id(buffer);

let updateBuffer = (~buffer, editor) => {
  {
    ...editor,
    buffer,
    // TODO: These will both change with word wrap
    viewLines: EditorBuffer.numberOfLines(buffer),
    maxLineLength: EditorBuffer.getEstimatedMaxLineLength(buffer),
  };
};

let addInlineElement = (~uniqueId, ~lineNumber, ~height, editor) => {
  {
    ...editor,
    inlineElements:
      InlineElements.add(
        ~uniqueId,
        ~line=lineNumber,
        ~height=float(height),
        editor.inlineElements,
      ),
  };
};

let removeInlineElement = (~uniqueId, editor) => {
  {
    ...editor,
    inlineElements: InlineElements.remove(~uniqueId, editor.inlineElements),
  };
};

module Slow = {
  let pixelPositionToBytePosition =
      (~buffer, ~pixelX: float, ~pixelY: float, view) => {

    let rawLine =  getLineFromPixelY(~pixelY=pixelY +. view.scrollY, view);

    prerr_endline ("rawline: " ++ string_of_int(rawLine));

    let totalLinesInBuffer = Buffer.getNumberOfLines(buffer);

    let lineIdx =
      if (rawLine >= totalLinesInBuffer) {
        max(0, totalLinesInBuffer - 1);
      } else {
        rawLine;
      };

    if (lineIdx >= 0 && lineIdx < totalLinesInBuffer) {
      let bufferLine = Buffer.getLine(lineIdx, buffer);
      let index =
        BufferLine.Slow.getIndexFromPixel(
          ~pixel=pixelX +. view.scrollX,
          bufferLine,
        );

      let byteIndex = BufferLine.getByteFromIndex(~index, bufferLine);

      BytePosition.{
        line: EditorCoreTypes.LineNumber.ofZeroBased(lineIdx),
        byte: byteIndex,
      };
    } else {
      BytePosition.{
        line: EditorCoreTypes.LineNumber.zero,
        byte: ByteIndex.zero,
      };
    };
  };
};
