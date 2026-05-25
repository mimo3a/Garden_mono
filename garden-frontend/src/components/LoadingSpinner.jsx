export default function LoadingSpinner() {
  return (
    <div className="flex justify-center items-center py-20">
      <div className="w-8 h-8 border-2 border-green-500 border-t-transparent rounded-full animate-spin" />
    </div>
  )
}
